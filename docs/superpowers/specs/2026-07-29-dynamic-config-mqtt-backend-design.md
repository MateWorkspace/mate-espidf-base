# Dynamic Config via MQTT + Backend Storage Design

## Context

This is sub-project 2 of a 3-part dynamic-configuration effort. Sub-project
1 (complete, merged to `mate-espidf-base`'s `development`) added a
compiled-in, BLE-exposed schema of the device's preloaded config keys and
their value types: `main/include/domain/models/preloaded.h` now has
`DOMAIN_MODELS_PRELOADED_SCHEMA(X)` (an X-macro listing each key's name,
NVS key string, and `dom_models_preloaded_value_type_t`), materialized as
`dom_models_preloaded_schema[]` and exposed over a new BLE GATT service.

Today, config values (`mqtt_proto`, `mqtt_host`, `mqtt_port`, `mqtt_user`,
`mqtt_pass`, `system_restart_after_ms`, `wifi_sta_try_connect_on_init`) can
only be changed over BLE, via the `settings` usecase's `set_preloaded`.
There is no way to push a config change from the backend, and the backend
has no record of what config keys a given firmware supports or what
values a given device currently has.

This sub-project adds: (1) an MQTT topic `/sub/{device_id}/config` so the
backend can push a single config key/value change to a device, using the
same `settings->set_preloaded` codepath BLE already uses; (2) two new
backend tables recording, per firmware, which config keys exist and their
types, and per node, its current config values; (3) automatic ingestion of
a firmware's config schema at upload time, via `upload.py` parsing
`preloaded.h`'s X-macro and sending it alongside the binary.

Decisions already confirmed:
- MQTT config payload is a single `{key, value}` pair per publish
  (mirrors the existing `action` topic's one-shot-command shape; maps
  directly onto `set_preloaded`'s partial-update shape).
- No ack topic (mirrors `action`/`ota`, neither of which has one).
- Values stored as `TEXT` in the DB, parsed/validated against the
  recorded `value_type` at read/write time (avoids 3 mostly-NULL typed
  columns for a case that's rarely mixed).
- `upload.py` parses `preloaded.h` locally and sends the schema as JSON
  alongside the existing binary upload (no firmware/binary format
  changes; `upload.py` already runs from the source tree with direct
  access to the plain-text header).

## 1. Backend — database

Two new migrations, following this repo's existing conventions (UUID PK
via `gen_random_uuid()`, soft-delete + audit columns, trigram/FK indexes,
`down.sql` reversing exactly what `up.sql` created):

```sql
-- create_firmware_config_parameters.up.sql
CREATE TABLE firmware_config_parameters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid (),
    firmware_id UUID NOT NULL REFERENCES firmwares (id),
    key TEXT NOT NULL,
    value_type TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    created_by UUID,
    updated_by UUID,
    deleted_by UUID,
    CONSTRAINT uq_firmware_config_parameters_firmware_id_key UNIQUE (firmware_id, key)
);

CREATE INDEX idx_firmware_config_parameters_firmware_id ON firmware_config_parameters (firmware_id);

CREATE INDEX idx_firmware_config_parameters_deleted_at ON firmware_config_parameters (deleted_at);
```

```sql
-- create_node_config_values.up.sql
CREATE TABLE node_config_values (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid (),
    node_id UUID NOT NULL REFERENCES nodes (id),
    firmware_id UUID NOT NULL REFERENCES firmwares (id),
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    created_by UUID,
    updated_by UUID,
    deleted_by UUID,
    CONSTRAINT uq_node_config_values_node_id_key UNIQUE (node_id, key)
);

CREATE INDEX idx_node_config_values_node_id ON node_config_values (node_id);

CREATE INDEX idx_node_config_values_firmware_id ON node_config_values (firmware_id);

CREATE INDEX idx_node_config_values_deleted_at ON node_config_values (deleted_at);
```

`node_config_values.firmware_id` records which firmware's schema a value
was validated against at write time — it is lineage/audit data, not a
join key for reads (reads join through `nodes.firmware_id` for the
node's *current* firmware, same as `firmware_management.ReadAvailableByNodeId`
already does).

`value_type` is stored as free `TEXT` (`"string"`/`"uint32"`/`"bool"`),
matching the wire representation already established by sub-project 1's
BLE `config_schema` characteristic — no new Go enum type needed, the
three literal strings are validated at the application layer.

## 2. Backend — domain models

`internal/domain/models/firmware_config_parameter.go`:
```go
type FirmwareConfigParameter struct {
	Id         uuid.UUID  `db:"id" json:"id"`
	FirmwareId uuid.UUID  `db:"firmware_id" json:"firmware_id"`
	Key        string     `db:"key" json:"key"`
	ValueType  string     `db:"value_type" json:"value_type"`
	CreatedAt  time.Time  `db:"created_at" json:"created_at"`
	UpdatedAt  *time.Time `db:"updated_at" json:"updated_at,omitempty"`
	DeletedAt  *time.Time `db:"deleted_at" json:"deleted_at,omitempty"`
	CreatedBy  *uuid.UUID `db:"created_by" json:"created_by,omitempty"`
	UpdatedBy  *uuid.UUID `db:"updated_by" json:"updated_by,omitempty"`
	DeletedBy  *uuid.UUID `db:"deleted_by" json:"deleted_by,omitempty"`
}
```

`internal/domain/models/node_config_value.go`:
```go
type NodeConfigValue struct {
	Id         uuid.UUID  `db:"id" json:"id"`
	NodeId     uuid.UUID  `db:"node_id" json:"node_id"`
	FirmwareId uuid.UUID  `db:"firmware_id" json:"firmware_id"`
	Key        string     `db:"key" json:"key"`
	Value      string     `db:"value" json:"value"`
	CreatedAt  time.Time  `db:"created_at" json:"created_at"`
	UpdatedAt  *time.Time `db:"updated_at" json:"updated_at,omitempty"`
	DeletedAt  *time.Time `db:"deleted_at" json:"deleted_at,omitempty"`
	CreatedBy  *uuid.UUID `db:"created_by" json:"created_by,omitempty"`
	UpdatedBy  *uuid.UUID `db:"updated_by" json:"updated_by,omitempty"`
	DeletedBy  *uuid.UUID `db:"deleted_by" json:"deleted_by,omitempty"`
}
```

## 3. Backend — repository contracts + postgres impl

Following the `firmware`/`node` repository pattern exactly
(`domain/contracts/repository/firmware_config_parameter.go`,
`domain/contracts/repository/node_config_value.go`, each with a
`postgres.go` + `postgres_query.go` pair under
`infrastructure/repository/{firmware_config_parameter,node_config_value}/`):

```go
type FirmwareConfigParameter interface {
	ReplaceForFirmwareId(ctx context.Context, firmwareId uuid.UUID, params []domainmodels.FirmwareConfigParameter) error
	ReadByFirmwareId(ctx context.Context, firmwareId uuid.UUID) ([]domainmodels.FirmwareConfigParameter, error)
}

type NodeConfigValue interface {
	ReadByNodeId(ctx context.Context, nodeId uuid.UUID) ([]domainmodels.NodeConfigValue, error)
	ReadByNodeIdAndKey(ctx context.Context, nodeId uuid.UUID, key string) (*domainmodels.NodeConfigValue, error)
	Upsert(ctx context.Context, nodeId uuid.UUID, firmwareId uuid.UUID, key string, value string, actorId *uuid.UUID) error
}
```

`ReplaceForFirmwareId` runs inside a single transaction: soft-delete every
existing row for that `firmware_id` not present in the new list, then
upsert (`ON CONFLICT (firmware_id, key) DO UPDATE`) each entry in the new
list. This gives "replace-all" semantics for a re-upload that changes the
schema, without losing history via hard delete (consistent with this
repo's soft-delete convention everywhere else).

`Upsert` uses `ON CONFLICT (node_id, key) DO UPDATE SET value = ..., firmware_id = ..., updated_at = NOW(), updated_by = ...`.

Repository caching: this repo wraps some repositories in a Redis cache
layer (`domain/usecases/repocache/{firmware,node}.go`). These two new
repositories are NOT cached — config values change relatively often
(that's the point of this feature) and are read infrequently enough
(admin UI, not a hot device-facing path) that cache invalidation
complexity isn't justified. Direct repository access from the usecases.

## 4. Backend — application usecases

`application/node/config_parameter/usecase.go` implementing
`domain/usecases/node/config_parameter.go`:

```go
type ConfigParameter interface {
	ReplaceForFirmware(ctx context.Context, request ReplaceConfigParametersRequest) error
	ReadByFirmwareId(ctx context.Context, request ReadConfigParametersByFirmwareIdRequest) ([]domainmodels.FirmwareConfigParameter, error)
}

type ReplaceConfigParametersRequest struct {
	FirmwareId uuid.UUID
	Parameters []ConfigParameterInput  // Key, ValueType
}

type ConfigParameterInput struct {
	Key       string
	ValueType string
}

type ReadConfigParametersByFirmwareIdRequest struct {
	FirmwareId uuid.UUID
}
```

`ReplaceForFirmware` validates each `ValueType` is one of
`"string"`/`"uint32"`/`"bool"` (reject the whole batch with
`ErrTypeValidation` otherwise — matches this repo's existing
fail-the-whole-request-on-bad-input convention, e.g.
`applicationshared.RequiredFirmwareName`), then delegates to the
repository's `ReplaceForFirmwareId`.

`application/node/config_value/usecase.go` implementing
`domain/usecases/node/config_value.go`:

```go
type ConfigValue interface {
	ReadByNodeId(ctx context.Context, request ReadConfigValuesByNodeIdRequest) ([]domainmodels.NodeConfigValue, error)
	SetByNodeId(ctx context.Context, request SetConfigValueRequest) error
}

type ReadConfigValuesByNodeIdRequest struct {
	NodeId uuid.UUID
}

type SetConfigValueRequest struct {
	NodeId  uuid.UUID
	Key     string
	Value   string
	ActorId *uuid.UUID
}
```

`SetByNodeId`:
1. Reads the node (`domainusecasesrepocache.Node.ReadById`) to get
   `NodeClassId`/`FirmwareId`/`DeviceId`.
2. Reads that firmware's parameters (`ConfigParameter.ReadByFirmwareId`)
   and finds the entry matching `request.Key` — `ErrTypeNotFound` if the
   key isn't in that firmware's schema (mirrors `firmware_management`'s
   own not-found handling).
3. Type-checks `request.Value` against the matched `ValueType`:
   `"uint32"` must parse via `strconv.ParseUint(value, 10, 32)`,
   `"bool"` must be exactly `"true"` or `"false"`, `"string"` accepts
   anything. `ErrTypeValidation` on mismatch.
4. Calls `NodeConfigValue.Upsert(...)`.
5. Calls `nodePublish.Config(ctx, node.DeviceId, request.Key, request.Value)`
   (new method on the existing `domain/contracts/node/publish.go`
   `Publish` interface — added alongside `RegistrationAck`/`Ota`/`Action`).
6. Logs and returns the publish error if the MQTT publish itself fails
   (matching how `Ota`/`Action` publish failures already propagate) —
   the DB write is NOT rolled back on publish failure; the value is
   already correctly recorded, and the device will pick it up next time
   it's read/pushed (out of scope to add a retry-queue here — YAGNI for
   this sub-project, matching the fire-and-forget precedent already set
   by `action`/`ota`).

`firmware_management`'s `Create` and `ReplaceBinaryById` (existing
usecase, unchanged interface shape except two new *optional* fields on
their request structs — see below) call
`ConfigParameter.ReplaceForFirmware` after a successful binary store +
firmware row write, passing through whatever schema was included in the
upload request. If no schema was included (nil/empty — covers any
existing caller that predates this feature), `ReplaceForFirmware` is
skipped entirely; existing parameter rows for that firmware, if any, are
left untouched.

`CreateFirmwareRequest` and `ReplaceFirmwareBinaryByIdRequest`
(`domain/usecases/node/firmware_management.go`) each gain:
```go
ConfigSchema []ConfigParameterInput  // nil/empty: no schema change
```
(`ConfigParameterInput` reused from the `config_parameter` package.)

## 5. Backend — Publish interface + MQTT infra

`domain/contracts/node/publish.go`'s `Publish` interface gains:
```go
Config(
    ctx context.Context,
    nodeDeviceId string,
    key string,
    value string,
) (err error)
```

`infrastructure/node/publish/mqtt.go`'s `mqttImpl` gains a matching
`Config` method, `configQos = 1` constant (matching `actionQos`/`otaQos`),
publishing to `infrastructurenodeshared.NodeSubTopic(nodeDeviceId, "config")`
with a JSON body built from a new
`infrastructure/node/shared/payload.go` type:
```go
type ConfigPayload struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}
```

## 6. Backend — HTTP API

Added to the existing `presentation/http/handler/node` package (matches
how OTA dispatch already lives there despite being its own concern):

- `GET /v1/firmwares/{id}/config-parameters` — `permission("firmware:get")`
  (reuses the existing firmware-read permission; this is firmware
  metadata, not a new resource class). Returns
  `[]presentationhttpresponse.FirmwareConfigParameter` (`key`,
  `value_type`).
- `GET /v1/nodes/{id}/config` — new `permission("node_config:get")`.
  Returns `[]presentationhttpresponse.NodeConfigValue` (`key`, `value`,
  `updated_at`).
- `PUT /v1/nodes/{id}/config` — new `permission("node_config:set")`. Body
  `presentationhttprequest.SetNodeConfigValueRequest{ Key, Value string }`.
  Calls `ConfigValue.SetByNodeId`. Returns `204 No Content` (matches the
  OTA dispatch endpoints' response shape).

New permissions added to `database/seeder/permission.json`:
```json
{ "name": "node_config:get", "description": "View a node's config values." },
{ "name": "node_config:set", "description": "Set a node's config value." },
```

Swagger annotations follow the existing `@Failure` pattern established by
the error-response-redesign work (400/401/403/404/500, plus 412 if a
usecase can return `ErrTypeBadState` — not applicable here since none of
these usecases have a bad-state path).

## 7. Firmware — new MQTT config subscription

Mirrors the existing `action`/`ota` topic pattern exactly (all four
layers already established for those two topics get one more branch
each):

**`domain/contracts/messaging/def_sub.h`**: add
```c
dom_models_error_t (*config)(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
```

**`infrastructure/messaging/def_sub/mqtt_impl.c`** (and `stub_impl.c`):
add `config_impl` calling
`inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "config")`,
wired into `self->config = config_impl;` in `_new`.

**`application/internal/messaging_callbacks`**: `subscribe_defaults_impl`
adds a call to `ctx->cfg.def_sub->config(device_id_str, ctx->cfg.def_sub)`
alongside the existing `ota`/`action` subscribe calls, with the same
error-log-and-return-early pattern.

**`presentation/mqtt/context.h`/`.c`**: `pres_mqtt_context_t` gains a
`char config_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];` field, precomputed
in `_init` alongside `registration_ack_topic`/`ota_topic`/`action_topic`.
Also gains `dom_usecases_internal_settings_t* settings;` (the existing
usecase from sub-project 1's refactor context, needed so the new config
handler can call `set_preloaded`) — added as a new `pres_mqtt_context_new`
parameter, threaded through from `composition/main/presentation.c`
(`launcher->application.settings`).

**`presentation/mqtt/event/on_message.c`**: one more
`else if (strcmp(ctx->topic_scratch, ctx->config_topic) == 0)` branch
calling the new handler.

**New `presentation/mqtt/handler/config/{dto,handler}.{h,c}`** (mirrors
`presentation/mqtt/handler/action/{dto,handler}.{h,c}` exactly):

`dto.h`/`dto.c`: `pres_mqtt_handler_config_dto_decode(data, data_len, out)`
where `out` is `pres_mqtt_handler_config_dto_request_t { char key[32]; bool key_set; char value[128]; bool value_set; }` — parses `{"key":"...","value":"..."}`
(both fields always decoded as raw strings at the DTO layer; type-specific
parsing happens in the handler using sub-project 1's schema, not here —
keeps the DTO layer a pure JSON-shape concern, consistent with every
other DTO in this codebase).

`handler.c`'s `pres_mqtt_handler_config`:
1. Decode; log-and-return on missing `key`/`value` (matching action
   handler's per-field validation).
2. Linear-scan `dom_models_preloaded_schema[0..DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT)`
   for an entry whose `.key` matches the decoded `key`. Not found → warn
   "Unknown config key" and return (matching action handler's "unknown
   action" precedent — no ack exists to report this back).
3. Build a `dom_usecases_internal_settings_preloaded_update_t`, zeroed,
   then set exactly the one matching field + its `_set` flag based on
   the schema entry's `dom_models_preloaded_value_type_t`:
   `STRING` → `strncpy` into the matching `char[]` field by comparing
   the schema key string against each of the 5 known string keys (a
   small `if`/`else if` chain on the key string — the update struct has
   named fields, not a generic map, so this mapping is written by hand
   once, same as the BLE settings handler already does implicitly via
   its DTO's `copy_field_if_present` calls); `UINT32` →
   `strtoul(value, NULL, 10)` into `system_restart_after_ms`; `BOOL` →
   `strcmp(value, "true") == 0` into `wifi_sta_try_connect_on_init`.
   Malformed numeric/bool value → warn "Invalid config value" and
   return, no partial update applied.
4. Call `ctx->settings->set_preloaded(ctx->settings, &update, &restart_required_out)`
   (ignore `restart_required_out`'s value — MQTT has no restart-required
   notification path the way BLE's `restart_required` characteristic
   does; out of scope to add one here). Log success/failure the same way
   the settings BLE handler's update path does (usecase already logs
   failures internally — no double-log, per this codebase's established
   log-once convention).

## 8. `upload.py` — automatic schema parsing

`upload.py` gains a `parse_preloaded_schema(repo_root: Path) -> list[dict]`
function that reads
`main/include/domain/models/preloaded.h`, regex-extracts each
`X(NAME, DOMAIN_MODELS_PRELOADED_..._KEY, DOMAIN_MODELS_PRELOADED_VALUE_TYPE_...)`
row inside the `DOMAIN_MODELS_PRELOADED_SCHEMA(X)` block, resolves each
row's middle argument back to its literal string via a second regex over
the `#define ..._KEY "..."` lines (both regexes operate on the same
already-open file's text), and maps the third argument's enum suffix
(`STRING`/`UINT32`/`BOOL`) to the lowercase wire string used everywhere
else in this design (`"string"`/`"uint32"`/`"bool"`). Result:
`[{"key": "mqtt_proto", "type": "string"}, ...]`.

`create_firmware`/`replace_firmware_binary` (both existing functions in
`upload.py`) gain a `config_schema` form field, JSON-encoded from this
parsed list, sent alongside the existing `data`/`files` multipart
payload on both the `POST /v1/firmwares` and
`PUT /v1/firmwares/{id}/binary` calls. This makes schema ingestion fully
automatic — no `upload.config.json` change needed, no manual step.

## Out of scope

- Any UI for viewing/editing node config values (this sub-project is
  API-only, matching how OTA dispatch and firmware management also have
  no frontend work described here).
- A restart-required notification path for MQTT-driven config changes
  (BLE has one via its `restart_required` characteristic; MQTT does not
  gain an equivalent here).
- Retry/queueing for a config value that's written to the DB but fails
  to publish over MQTT (fire-and-forget, matching `action`/`ota`).
- Sub-project 3's upload-tooling hygiene (`.gitignore`, `node_class_name`
  existence validation, `firmware_name` automation) — separate spec.
- Caching the two new repositories.

## Verification

Firmware side: no unit-test suite exists (per established convention);
verification is `idf.py build` plus hardware-in-loop MQTT testing (direct
`paho-mqtt` publish to `/sub/{device_id}/config`, same approach used for
prior MQTT scenarios this session, e.g. the registration_ack check),
confirming a device correctly updates one of its 7 preloaded values and
rejects an unknown key or malformed value without crashing.

Backend side: `go build ./...`, `go vet ./...`, then
`docker compose up --build` and exercise the new endpoints end-to-end:
upload a firmware via `upload.py` and confirm `GET
/v1/firmwares/{id}/config-parameters` returns the 7 real parameters;
`PUT /v1/nodes/{id}/config` with a valid key/value and confirm `GET
/v1/nodes/{id}/config` reflects it and the device (or a `paho-mqtt`
subscriber standing in for one) receives the MQTT publish; a request with
an unknown key or wrong-typed value returns 400/404 appropriately.
