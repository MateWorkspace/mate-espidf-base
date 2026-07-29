# Dynamic Config via MQTT + Backend Storage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the backend push a single config key/value change to a device over MQTT (`/sub/{device_id}/config`), backed by two new backend tables tracking each firmware's config schema and each node's current config values, with the schema learned automatically at firmware-upload time.

**Architecture:** Backend gains `firmware_config_parameters`/`node_config_values` tables, matching repository/usecase/HTTP layers, and a `Publish.Config` MQTT method. `upload.py` parses `mate-espidf-base`'s `preloaded.h` schema and sends it with each upload. Firmware gains a `config` MQTT subscription (mirroring the existing `action`/`ota` topics exactly) whose handler maps an incoming `{key,value}` onto the existing `settings->set_preloaded` codepath, using sub-project 1's `dom_models_preloaded_schema` to validate the key and its type.

**Tech Stack:** Go 1.x (Echo v5, pgx/v5, squirrel, paho.mqtt.golang) for the backend at `/home/dodol/Repositories/mate/mate-things/backend`; ESP-IDF v6.0.2 C (NimBLE unrelated here, esp-mqtt) for the firmware at `/home/dodol/Repositories/mate/mate-espidf-base`. **This plan spans two separate git repositories** — every task states its working directory explicitly; there is no single build/commit spanning both.

## Global Constraints

- Backend: every new table has `id UUID PRIMARY KEY DEFAULT gen_random_uuid()`, standard audit columns (`created_at/updated_at/deleted_at/created_by/updated_by/deleted_by`), and a `preferences JSONB NOT NULL DEFAULT '{}'::jsonb` column — matches every existing table in `database/migrations/` (`firmwares`, `nodes`, `node_classes`, `payload_schemas`), even though the design spec didn't call it out explicitly.
- Backend: repository layer uses `github.com/Masterminds/squirrel` (Dollar placeholders, `p.SqrD`) + `pgx/v5`, wrapped in `infrastructurerepositoryshared.BasePostgres`; errors go through `infrastructurerepositoryshared.MapPgxError`/`QueryBuildError`/`NotFound`; row scanning via a `ScanPgxX`/`ScanPgxXs` pair in `infrastructure/repository/shared/scan_pgx.go`.
- Backend: a repository that is explicitly NOT Redis-cached (per the spec's decision) is used directly via a `domaincontractsrepository.X` interface — no `domainusecasesrepocache.X` wrapper, no cache contract/impl, matching `actionLogRepository`/`telemetryRecordRepository`'s existing precedent.
- Backend: HTTP errors always go through `domainmodels.NewError(message, errType, source)` + `presentationhttputils.Error(c, err, "user-facing message")`; validation helpers live in `application/shared/validation.go` (`RequiredX`/`OptionalX` pairs); request-shape helpers (`RequiredUUID`, `RequiredString`, `Bind`, `ActorId`) live in `presentation/http/utils/`.
- Backend: migrations are `YYYYMMDDHHMMSS_create_<table>.up.sql`/`.down.sql` pairs in `database/migrations/`, timestamp must sort after the existing latest (`20260727080000_...`).
- Backend: new permissions are added to `database/seeder/permission.json` following the exact `{ "name": "resource:action", "description": "..." }` shape.
- Firmware: module lifecycle convention (`_new`/`_init`/`_deinit`/`_delete` for stateful modules, `_new`/`_delete` only for stateless ones), `BASE_TAG "<module/path>"` + per-function `tag = BASE_TAG "/<fn>"` logging, `validate_cfg` always extracted to its own file — all established by the prior architecture-consistency-refactor and BLE system-info sub-project on this same firmware repo.
- Firmware: no unit-test suite exists; verification is `idf.py build` (`idf.py reconfigure` first whenever new source files are added) plus hardware-in-loop MQTT testing via `paho-mqtt` against the real ESP32-C3 on `/dev/ttyACM0` (IDF env: `source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"`).
- Firmware: the new `config` MQTT topic/handler must mirror the existing `action` topic's four-layer wiring (`domain/contracts/messaging/def_sub.h` → `infrastructure/messaging/def_sub/{mqtt_impl,stub_impl}.c` → `application/internal/messaging_callbacks`'s `subscribe_defaults_impl` → `presentation/mqtt/context.c`/`on_message.c` → new `presentation/mqtt/handler/config/`) exactly, changing only the topic suffix (`"config"`) and the payload shape/handling.
- MQTT config payload is always a single `{"key":"...","value":"..."}` pair; no ack topic exists for it (both confirmed decisions from the design spec).

---

## Backend tasks (repo: `/home/dodol/Repositories/mate/mate-things/backend`)

### Task 1: Migrations + domain models

**Files:**
- Create: `database/migrations/20260729130000_create_firmware_config_parameters.up.sql`
- Create: `database/migrations/20260729130000_create_firmware_config_parameters.down.sql`
- Create: `database/migrations/20260729130005_create_node_config_values.up.sql`
- Create: `database/migrations/20260729130005_create_node_config_values.down.sql`
- Create: `internal/domain/models/firmware_config_parameter.go`
- Create: `internal/domain/models/node_config_value.go`

**Interfaces:**
- Produces: `domainmodels.FirmwareConfigParameter{Id, FirmwareId, Key, ValueType, Preferences, CreatedAt, UpdatedAt, DeletedAt, CreatedBy, UpdatedBy, DeletedBy}` and `domainmodels.NodeConfigValue{Id, NodeId, FirmwareId, Key, Value, Preferences, CreatedAt, UpdatedAt, DeletedAt, CreatedBy, UpdatedBy, DeletedBy}`. Task 2 (repositories) consumes these exact field names/types.

- [ ] **Step 1: Write the `firmware_config_parameters` migration**

`database/migrations/20260729130000_create_firmware_config_parameters.up.sql`:
```sql
CREATE TABLE firmware_config_parameters (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid (),
    firmware_id UUID NOT NULL REFERENCES firmwares (id),
    key TEXT NOT NULL,
    value_type TEXT NOT NULL,
    preferences JSONB NOT NULL DEFAULT '{}'::jsonb,
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

`database/migrations/20260729130000_create_firmware_config_parameters.down.sql`:
```sql
DROP TABLE IF EXISTS firmware_config_parameters;
```

- [ ] **Step 2: Write the `node_config_values` migration**

`database/migrations/20260729130005_create_node_config_values.up.sql`:
```sql
CREATE TABLE node_config_values (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid (),
    node_id UUID NOT NULL REFERENCES nodes (id),
    firmware_id UUID NOT NULL REFERENCES firmwares (id),
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    preferences JSONB NOT NULL DEFAULT '{}'::jsonb,
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

`database/migrations/20260729130005_create_node_config_values.down.sql`:
```sql
DROP TABLE IF EXISTS node_config_values;
```

- [ ] **Step 3: Write the domain models**

`internal/domain/models/firmware_config_parameter.go`:
```go
package domainmodels

import (
	"encoding/json"
	"time"

	"github.com/google/uuid"
)

type FirmwareConfigParameter struct {
	Id          uuid.UUID       `db:"id" json:"id"`
	FirmwareId  uuid.UUID       `db:"firmware_id" json:"firmware_id"`
	Key         string          `db:"key" json:"key"`
	ValueType   string          `db:"value_type" json:"value_type"`
	Preferences json.RawMessage `db:"preferences" json:"preferences"`
	CreatedAt   time.Time       `db:"created_at" json:"created_at"`
	UpdatedAt   *time.Time      `db:"updated_at" json:"updated_at,omitempty"`
	DeletedAt   *time.Time      `db:"deleted_at" json:"deleted_at,omitempty"`
	CreatedBy   *uuid.UUID      `db:"created_by" json:"created_by,omitempty"`
	UpdatedBy   *uuid.UUID      `db:"updated_by" json:"updated_by,omitempty"`
	DeletedBy   *uuid.UUID      `db:"deleted_by" json:"deleted_by,omitempty"`
}
```

`internal/domain/models/node_config_value.go`:
```go
package domainmodels

import (
	"encoding/json"
	"time"

	"github.com/google/uuid"
)

type NodeConfigValue struct {
	Id          uuid.UUID       `db:"id" json:"id"`
	NodeId      uuid.UUID       `db:"node_id" json:"node_id"`
	FirmwareId  uuid.UUID       `db:"firmware_id" json:"firmware_id"`
	Key         string          `db:"key" json:"key"`
	Value       string          `db:"value" json:"value"`
	Preferences json.RawMessage `db:"preferences" json:"preferences"`
	CreatedAt   time.Time       `db:"created_at" json:"created_at"`
	UpdatedAt   *time.Time      `db:"updated_at" json:"updated_at,omitempty"`
	DeletedAt   *time.Time      `db:"deleted_at" json:"deleted_at,omitempty"`
	CreatedBy   *uuid.UUID      `db:"created_by" json:"created_by,omitempty"`
	UpdatedBy   *uuid.UUID      `db:"updated_by" json:"updated_by,omitempty"`
	DeletedBy   *uuid.UUID      `db:"deleted_by" json:"deleted_by,omitempty"`
}
```

- [ ] **Step 4: Run the migrations against a local/dev database and verify**

```bash
cd /home/dodol/Repositories/mate/mate-things
migrate -path backend/database/migrations -database "$DATABASE_URL" up
```
Expected: no errors; `\d firmware_config_parameters` and `\d node_config_values` in `psql` show the expected columns/indexes/constraints.

- [ ] **Step 5: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```
Expected: clean (the two new model files have no other code referencing them yet, so this only proves they parse/compile).

- [ ] **Step 6: Commit**

```bash
git add database/migrations/20260729130000_create_firmware_config_parameters.up.sql \
        database/migrations/20260729130000_create_firmware_config_parameters.down.sql \
        database/migrations/20260729130005_create_node_config_values.up.sql \
        database/migrations/20260729130005_create_node_config_values.down.sql \
        internal/domain/models/firmware_config_parameter.go \
        internal/domain/models/node_config_value.go
git commit -m "feat: add firmware_config_parameters and node_config_values tables"
```

---

### Task 2: Repository contracts + postgres implementations

**Files:**
- Create: `internal/domain/contracts/repository/firmware_config_parameter.go`
- Create: `internal/domain/contracts/repository/node_config_value.go`
- Create: `internal/infrastructure/repository/firmware_config_parameter/postgres.go`
- Create: `internal/infrastructure/repository/firmware_config_parameter/postgres_query.go`
- Create: `internal/infrastructure/repository/node_config_value/postgres.go`
- Create: `internal/infrastructure/repository/node_config_value/postgres_query.go`
- Modify: `internal/infrastructure/repository/shared/scan_pgx.go`

**Interfaces:**
- Consumes: `domainmodels.FirmwareConfigParameter`/`NodeConfigValue` (Task 1).
- Produces: `domaincontractsrepository.FirmwareConfigParameter{ReplaceForFirmwareId, ReadByFirmwareId}`, `domaincontractsrepository.NodeConfigValue{ReadByNodeId, ReadByNodeIdAndKey, Upsert}`, and their `infrastructurerepositoryfirmwareconfigparameter.NewPostgresImpl(...)`/`infrastructurerepositorynodeconfigvalue.NewPostgresImpl(...)` constructors. Task 4 (application usecases) and Task 6 (composition wiring) consume these exact names.

- [ ] **Step 1: Add scan helpers**

Append to `internal/infrastructure/repository/shared/scan_pgx.go` (after the existing `ScanPgxActionLogs` function, same file):
```go
func ScanPgxFirmwareConfigParameter(row pgx.Row) (domainmodels.FirmwareConfigParameter, error) {
	var item domainmodels.FirmwareConfigParameter
	err := row.Scan(
		&item.Id,
		&item.FirmwareId,
		&item.Key,
		&item.ValueType,
		&item.Preferences,
		&item.CreatedAt,
		&item.UpdatedAt,
		&item.DeletedAt,
		&item.CreatedBy,
		&item.UpdatedBy,
		&item.DeletedBy,
	)
	return item, err
}

func ScanPgxFirmwareConfigParameters(rows pgx.Rows) ([]domainmodels.FirmwareConfigParameter, error) {
	items := make([]domainmodels.FirmwareConfigParameter, 0)
	for rows.Next() {
		item, err := ScanPgxFirmwareConfigParameter(rows)
		if err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, rows.Err()
}

func ScanPgxNodeConfigValue(row pgx.Row) (domainmodels.NodeConfigValue, error) {
	var item domainmodels.NodeConfigValue
	err := row.Scan(
		&item.Id,
		&item.NodeId,
		&item.FirmwareId,
		&item.Key,
		&item.Value,
		&item.Preferences,
		&item.CreatedAt,
		&item.UpdatedAt,
		&item.DeletedAt,
		&item.CreatedBy,
		&item.UpdatedBy,
		&item.DeletedBy,
	)
	return item, err
}

func ScanPgxNodeConfigValues(rows pgx.Rows) ([]domainmodels.NodeConfigValue, error) {
	items := make([]domainmodels.NodeConfigValue, 0)
	for rows.Next() {
		item, err := ScanPgxNodeConfigValue(rows)
		if err != nil {
			return nil, err
		}
		items = append(items, item)
	}
	return items, rows.Err()
}
```

- [ ] **Step 2: Write the `FirmwareConfigParameter` repository contract**

`internal/domain/contracts/repository/firmware_config_parameter.go`:
```go
package domaincontractsrepository

import (
	"context"

	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	"github.com/google/uuid"
)

type FirmwareConfigParameter interface {
	// ReplaceForFirmwareId soft-deletes every existing row for firmwareId
	// not present in params, then upserts each entry in params
	// (ON CONFLICT (firmware_id, key) DO UPDATE), all in one transaction.
	ReplaceForFirmwareId(
		ctx context.Context,
		firmwareId uuid.UUID,
		params []FirmwareConfigParameterInput,
		actorId *uuid.UUID,
	) (err error)

	ReadByFirmwareId(
		ctx context.Context,
		firmwareId uuid.UUID,
	) (params []domainmodels.FirmwareConfigParameter, err error)
}

type FirmwareConfigParameterInput struct {
	Key       string
	ValueType string
}
```

- [ ] **Step 3: Write the `NodeConfigValue` repository contract**

`internal/domain/contracts/repository/node_config_value.go`:
```go
package domaincontractsrepository

import (
	"context"

	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	"github.com/google/uuid"
)

type NodeConfigValue interface {
	ReadByNodeId(
		ctx context.Context,
		nodeId uuid.UUID,
	) (values []domainmodels.NodeConfigValue, err error)

	ReadByNodeIdAndKey(
		ctx context.Context,
		nodeId uuid.UUID,
		key string,
	) (value *domainmodels.NodeConfigValue, err error)

	Upsert(
		ctx context.Context,
		nodeId uuid.UUID,
		firmwareId uuid.UUID,
		key string,
		value string,
		actorId *uuid.UUID,
	) (err error)
}
```

- [ ] **Step 4: Write the `firmware_config_parameter` postgres implementation**

`internal/infrastructure/repository/firmware_config_parameter/postgres.go`:
```go
package infrastructurerepositoryfirmwareconfigparameter

import (
	"context"
	"errors"

	"github.com/Masterminds/squirrel"
	domaincontractsrepository "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/repository"
	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	infrastructurerepositoryshared "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/shared"
	"github.com/MateWorkspace/mate-things/backend/pkg/pgxdt"
	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type postgresImpl struct {
	infrastructurerepositoryshared.BasePostgres
}

func NewPostgresImpl(
	dt pgxdt.Pgxdt,
	sqrQuestion *squirrel.StatementBuilderType,
	sqrDollar *squirrel.StatementBuilderType,
) domaincontractsrepository.FirmwareConfigParameter {
	return &postgresImpl{
		BasePostgres: infrastructurerepositoryshared.BasePostgres{
			Dt:   dt,
			SqrQ: sqrQuestion,
			SqrD: sqrDollar,
		},
	}
}

func (p *postgresImpl) ReplaceForFirmwareId(
	ctx context.Context,
	firmwareId uuid.UUID,
	params []domaincontractsrepository.FirmwareConfigParameterInput,
	actorId *uuid.UUID,
) error {
	keys := make([]string, 0, len(params))
	for _, param := range params {
		keys = append(keys, param.Key)
	}

	deleteQuery, deleteArgs, err := p.querySoftDeleteMissing(firmwareId, keys, actorId)
	if err != nil {
		return infrastructurerepositoryshared.QueryBuildError("failed to build delete firmware config parameters query", err)
	}
	if _, err := p.Dt.Exec(ctx, deleteQuery, deleteArgs...); err != nil {
		return infrastructurerepositoryshared.MapPgxError("failed to delete stale firmware config parameters", err)
	}

	for _, param := range params {
		upsertQuery, upsertArgs, err := p.queryUpsert(firmwareId, param.Key, param.ValueType, actorId)
		if err != nil {
			return infrastructurerepositoryshared.QueryBuildError("failed to build upsert firmware config parameter query", err)
		}
		if _, err := p.Dt.Exec(ctx, upsertQuery, upsertArgs...); err != nil {
			return infrastructurerepositoryshared.MapPgxError("failed to upsert firmware config parameter", err)
		}
	}

	return nil
}

func (p *postgresImpl) ReadByFirmwareId(
	ctx context.Context,
	firmwareId uuid.UUID,
) ([]domainmodels.FirmwareConfigParameter, error) {
	query, args, err := p.queryReadByFirmwareId(firmwareId)
	if err != nil {
		return nil, infrastructurerepositoryshared.QueryBuildError("failed to build read firmware config parameters query", err)
	}

	rows, err := p.Dt.Query(ctx, query, args...)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return []domainmodels.FirmwareConfigParameter{}, nil
		}
		return nil, infrastructurerepositoryshared.MapPgxError("failed to read firmware config parameters", err)
	}
	defer rows.Close()

	items, err := infrastructurerepositoryshared.ScanPgxFirmwareConfigParameters(rows)
	if err != nil {
		return nil, infrastructurerepositoryshared.MapPgxError("failed to scan firmware config parameters", err)
	}

	return items, nil
}
```

`internal/infrastructure/repository/firmware_config_parameter/postgres_query.go`:
```go
package infrastructurerepositoryfirmwareconfigparameter

import (
	"github.com/Masterminds/squirrel"
	infrastructurerepositoryshared "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/shared"
	"github.com/google/uuid"
)

var firmwareConfigParameterColumns = []string{
	"id",
	"firmware_id",
	"key",
	"value_type",
	"preferences",
	"created_at",
	"updated_at",
	"deleted_at",
	"created_by",
	"updated_by",
	"deleted_by",
}

func (p *postgresImpl) queryReadByFirmwareId(firmwareId uuid.UUID) (query string, args []any, err error) {
	return p.SqrD.Select(firmwareConfigParameterColumns...).
		From("firmware_config_parameters").
		Where(squirrel.Eq{"firmware_id": firmwareId}).
		Where("deleted_at IS NULL").
		OrderBy("key ASC").
		ToSql()
}

func (p *postgresImpl) querySoftDeleteMissing(
	firmwareId uuid.UUID,
	keepKeys []string,
	actorId *uuid.UUID,
) (query string, args []any, err error) {
	q := p.SqrD.Update("firmware_config_parameters").
		Where(squirrel.Eq{"firmware_id": firmwareId}).
		Where("deleted_at IS NULL").
		Set("deleted_at", squirrel.Expr("CURRENT_TIMESTAMP")).
		Set("deleted_by", actorId)

	if len(keepKeys) > 0 {
		q = q.Where(squirrel.NotEq{"key": keepKeys})
	}

	return q.ToSql()
}

func (p *postgresImpl) queryUpsert(
	firmwareId uuid.UUID,
	key string,
	valueType string,
	actorId *uuid.UUID,
) (query string, args []any, err error) {
	return p.SqrD.Insert("firmware_config_parameters").
		Columns("firmware_id", "key", "value_type", "created_by").
		Values(firmwareId, key, valueType, actorId).
		Suffix("ON CONFLICT (firmware_id, key) DO UPDATE SET value_type = EXCLUDED.value_type, deleted_at = NULL, deleted_by = NULL, updated_at = CURRENT_TIMESTAMP, updated_by = ?", actorId).
		ToSql()
}
```

Note: `squirrel.NotEq{"key": keepKeys}` with a non-empty slice produces `key NOT IN (?, ?, ...)`, correctly excluding every key still present in the new upload from the soft-delete pass. When `keepKeys` is empty (a firmware upload with zero config parameters), the `Where` is skipped entirely, so the query becomes an unconditional soft-delete of every row for that firmware — correct, since an empty schema means none of the old parameters exist anymore.

- [ ] **Step 5: Write the `node_config_value` postgres implementation**

`internal/infrastructure/repository/node_config_value/postgres.go`:
```go
package infrastructurerepositorynodeconfigvalue

import (
	"context"
	"errors"

	"github.com/Masterminds/squirrel"
	domaincontractsrepository "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/repository"
	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	infrastructurerepositoryshared "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/shared"
	"github.com/MateWorkspace/mate-things/backend/pkg/pgxdt"
	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type postgresImpl struct {
	infrastructurerepositoryshared.BasePostgres
}

func NewPostgresImpl(
	dt pgxdt.Pgxdt,
	sqrQuestion *squirrel.StatementBuilderType,
	sqrDollar *squirrel.StatementBuilderType,
) domaincontractsrepository.NodeConfigValue {
	return &postgresImpl{
		BasePostgres: infrastructurerepositoryshared.BasePostgres{
			Dt:   dt,
			SqrQ: sqrQuestion,
			SqrD: sqrDollar,
		},
	}
}

func (p *postgresImpl) ReadByNodeId(ctx context.Context, nodeId uuid.UUID) ([]domainmodels.NodeConfigValue, error) {
	query, args, err := p.queryReadByNodeId(nodeId)
	if err != nil {
		return nil, infrastructurerepositoryshared.QueryBuildError("failed to build read node config values query", err)
	}

	rows, err := p.Dt.Query(ctx, query, args...)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return []domainmodels.NodeConfigValue{}, nil
		}
		return nil, infrastructurerepositoryshared.MapPgxError("failed to read node config values", err)
	}
	defer rows.Close()

	items, err := infrastructurerepositoryshared.ScanPgxNodeConfigValues(rows)
	if err != nil {
		return nil, infrastructurerepositoryshared.MapPgxError("failed to scan node config values", err)
	}

	return items, nil
}

func (p *postgresImpl) ReadByNodeIdAndKey(ctx context.Context, nodeId uuid.UUID, key string) (*domainmodels.NodeConfigValue, error) {
	query, args, err := p.queryReadByNodeIdAndKey(nodeId, key)
	if err != nil {
		return nil, infrastructurerepositoryshared.QueryBuildError("failed to build read node config value query", err)
	}

	item, err := infrastructurerepositoryshared.ScanPgxNodeConfigValue(p.Dt.QueryRow(ctx, query, args...))
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, infrastructurerepositoryshared.NotFound("node config value not found", err)
		}
		return nil, infrastructurerepositoryshared.MapPgxError("failed to read node config value", err)
	}

	return &item, nil
}

func (p *postgresImpl) Upsert(
	ctx context.Context,
	nodeId uuid.UUID,
	firmwareId uuid.UUID,
	key string,
	value string,
	actorId *uuid.UUID,
) error {
	query, args, err := p.queryUpsert(nodeId, firmwareId, key, value, actorId)
	if err != nil {
		return infrastructurerepositoryshared.QueryBuildError("failed to build upsert node config value query", err)
	}

	if _, err := p.Dt.Exec(ctx, query, args...); err != nil {
		return infrastructurerepositoryshared.MapPgxError("failed to upsert node config value", err)
	}

	return nil
}
```

`internal/infrastructure/repository/node_config_value/postgres_query.go`:
```go
package infrastructurerepositorynodeconfigvalue

import (
	"github.com/Masterminds/squirrel"
	infrastructurerepositoryshared "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/shared"
	"github.com/google/uuid"
)

var nodeConfigValueColumns = []string{
	"id",
	"node_id",
	"firmware_id",
	"key",
	"value",
	"preferences",
	"created_at",
	"updated_at",
	"deleted_at",
	"created_by",
	"updated_by",
	"deleted_by",
}

func (p *postgresImpl) queryReadByNodeId(nodeId uuid.UUID) (query string, args []any, err error) {
	return p.SqrD.Select(nodeConfigValueColumns...).
		From("node_config_values").
		Where(squirrel.Eq{"node_id": nodeId}).
		Where("deleted_at IS NULL").
		OrderBy("key ASC").
		ToSql()
}

func (p *postgresImpl) queryReadByNodeIdAndKey(nodeId uuid.UUID, key string) (query string, args []any, err error) {
	return p.SqrD.Select(nodeConfigValueColumns...).
		From("node_config_values").
		Where(squirrel.Eq{"node_id": nodeId, "key": key}).
		Where("deleted_at IS NULL").
		Limit(1).
		ToSql()
}

func (p *postgresImpl) queryUpsert(
	nodeId uuid.UUID,
	firmwareId uuid.UUID,
	key string,
	value string,
	actorId *uuid.UUID,
) (query string, args []any, err error) {
	return p.SqrD.Insert("node_config_values").
		Columns("node_id", "firmware_id", "key", "value", "created_by").
		Values(nodeId, firmwareId, key, value, actorId).
		Suffix("ON CONFLICT (node_id, key) DO UPDATE SET firmware_id = EXCLUDED.firmware_id, value = EXCLUDED.value, deleted_at = NULL, deleted_by = NULL, updated_at = CURRENT_TIMESTAMP, updated_by = ?", actorId).
		ToSql()
}
```

- [ ] **Step 6: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```

- [ ] **Step 7: Commit**

```bash
git add internal/domain/contracts/repository/firmware_config_parameter.go \
        internal/domain/contracts/repository/node_config_value.go \
        internal/infrastructure/repository/firmware_config_parameter \
        internal/infrastructure/repository/node_config_value \
        internal/infrastructure/repository/shared/scan_pgx.go
git commit -m "feat: add firmware_config_parameter and node_config_value repositories"
```

---

### Task 3: `Publish.Config` MQTT method

**Files:**
- Modify: `internal/domain/contracts/node/publish.go`
- Modify: `internal/infrastructure/node/publish/mqtt.go`
- Modify: `internal/infrastructure/node/shared/payload.go`

**Interfaces:**
- Produces: `domaincontractsnode.Publish.Config(ctx, nodeDeviceId, key, value string) error`. Task 4's `config_value` usecase consumes this.

- [ ] **Step 1: Add `Config` to the `Publish` interface**

In `internal/domain/contracts/node/publish.go`, add to the `Publish` interface (after `Action`):
```go
	Config(
		ctx context.Context,
		nodeDeviceId string,
		key string,
		value string,
	) (err error)
```

- [ ] **Step 2: Add the `ConfigPayload` type**

In `internal/infrastructure/node/shared/payload.go`, add (after `ActionPayload`):
```go
type ConfigPayload struct {
	Key   string `json:"key"`
	Value string `json:"value"`
}
```

- [ ] **Step 3: Implement `Config` in the MQTT publisher**

In `internal/infrastructure/node/publish/mqtt.go`:

Add a `configQos = 1` constant to the existing `const (...)` block (alongside `registrationAckQos`/`otaQos`/`actionQos`).

Add, after the existing `Action` method:
```go
func (m *mqttImpl) Config(
	ctx context.Context,
	nodeDeviceId string,
	key string,
	value string,
) (err error) {
	payload, err := json.Marshal(infrastructurenodeshared.ConfigPayload{
		Key:   key,
		Value: value,
	})
	if err != nil {
		return domainmodels.NewError("failed to build payload", domainmodels.ErrTypeValidation, err)
	}

	return m.publish(
		ctx, infrastructurenodeshared.NodeSubTopic(nodeDeviceId, "config"),
		configQos, false, payload,
	)
}
```

- [ ] **Step 4: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```

- [ ] **Step 5: Commit**

```bash
git add internal/domain/contracts/node/publish.go \
        internal/infrastructure/node/publish/mqtt.go \
        internal/infrastructure/node/shared/payload.go
git commit -m "feat: add Config method to the node Publish interface"
```

---

### Task 4: Application usecases — `config_parameter` and `config_value`

**Files:**
- Create: `internal/domain/usecases/node/config_parameter.go`
- Create: `internal/domain/usecases/node/config_value.go`
- Create: `internal/application/node/config_parameter/usecase.go`
- Create: `internal/application/node/config_value/usecase.go`

**Interfaces:**
- Consumes: `domaincontractsrepository.FirmwareConfigParameter`/`NodeConfigValue` (Task 2), `domaincontractsnode.Publish.Config` (Task 3), `domainusecasesrepocache.Node` (existing, for reading a node's `FirmwareId`/`DeviceId`).
- Produces: `domainusecasesnode.ConfigParameter{ReplaceForFirmware, ReadByFirmwareId}`, `domainusecasesnode.ConfigValue{ReadByNodeId, SetByNodeId}`. Task 5 (firmware_management wiring) and Task 6 (HTTP handlers, composition) consume these exact names.

- [ ] **Step 1: Write the `ConfigParameter` domain usecase interface**

`internal/domain/usecases/node/config_parameter.go`:
```go
package domainusecasesnode

import (
	"context"

	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	"github.com/google/uuid"
)

type ConfigParameter interface {
	ReplaceForFirmware(ctx context.Context, request ReplaceConfigParametersRequest) error
	ReadByFirmwareId(ctx context.Context, request ReadConfigParametersByFirmwareIdRequest) ([]domainmodels.FirmwareConfigParameter, error)
}

type ConfigParameterInput struct {
	Key       string
	ValueType string
}

type ReplaceConfigParametersRequest struct {
	FirmwareId uuid.UUID
	Parameters []ConfigParameterInput
	ActorId    *uuid.UUID
}

type ReadConfigParametersByFirmwareIdRequest struct {
	FirmwareId uuid.UUID
}
```

- [ ] **Step 2: Write the `ConfigValue` domain usecase interface**

`internal/domain/usecases/node/config_value.go`:
```go
package domainusecasesnode

import (
	"context"

	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	"github.com/google/uuid"
)

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

- [ ] **Step 3: Implement the `config_parameter` usecase**

`internal/application/node/config_parameter/usecase.go`:
```go
package applicationnodeconfigparameter

import (
	"context"

	domaincontractslogger "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/logger"
	domaincontractsrepository "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/repository"
	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	domainusecasesnode "github.com/MateWorkspace/mate-things/backend/internal/domain/usecases/node"
)

var validValueTypes = map[string]bool{
	"string": true,
	"uint32": true,
	"bool":   true,
}

type usecase struct {
	repository domaincontractsrepository.FirmwareConfigParameter
	logger     domaincontractslogger.Leveled
}

func NewUsecaseImpl(
	repository domaincontractsrepository.FirmwareConfigParameter,
	logger domaincontractslogger.Leveled,
) domainusecasesnode.ConfigParameter {
	return &usecase{
		repository: repository,
		logger:     logger,
	}
}

func (u *usecase) ReplaceForFirmware(ctx context.Context, request domainusecasesnode.ReplaceConfigParametersRequest) error {
	const tag = "node/config_parameter/ReplaceForFirmware"

	params := make([]domaincontractsrepository.FirmwareConfigParameterInput, 0, len(request.Parameters))
	for _, param := range request.Parameters {
		if !validValueTypes[param.ValueType] {
			return domainmodels.NewError("config parameter value_type must be one of string, uint32, bool", domainmodels.ErrTypeValidation, nil)
		}
		if param.Key == "" {
			return domainmodels.NewError("config parameter key is required", domainmodels.ErrTypeValidation, nil)
		}
		params = append(params, domaincontractsrepository.FirmwareConfigParameterInput{
			Key:       param.Key,
			ValueType: param.ValueType,
		})
	}

	if err := u.repository.ReplaceForFirmwareId(ctx, request.FirmwareId, params, request.ActorId); err != nil {
		u.logger.Error(ctx, tag, "failed to replace firmware config parameters", domainmodels.LoggerMeta{
			"err":         err,
			"firmware_id": request.FirmwareId,
		})
		return err
	}

	return nil
}

func (u *usecase) ReadByFirmwareId(
	ctx context.Context,
	request domainusecasesnode.ReadConfigParametersByFirmwareIdRequest,
) ([]domainmodels.FirmwareConfigParameter, error) {
	const tag = "node/config_parameter/ReadByFirmwareId"

	params, err := u.repository.ReadByFirmwareId(ctx, request.FirmwareId)
	if err != nil {
		u.logger.Error(ctx, tag, "failed to read firmware config parameters", domainmodels.LoggerMeta{
			"err":         err,
			"firmware_id": request.FirmwareId,
		})
		return nil, err
	}

	return params, nil
}
```

- [ ] **Step 4: Implement the `config_value` usecase**

`internal/application/node/config_value/usecase.go`:
```go
package applicationnodeconfigvalue

import (
	"context"
	"strconv"

	domaincontractslogger "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/logger"
	domaincontractsnode "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/node"
	domaincontractsrepository "github.com/MateWorkspace/mate-things/backend/internal/domain/contracts/repository"
	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
	domainusecasesnode "github.com/MateWorkspace/mate-things/backend/internal/domain/usecases/node"
	domainusecasesrepocache "github.com/MateWorkspace/mate-things/backend/internal/domain/usecases/repocache"
)

type usecase struct {
	repository          domaincontractsrepository.NodeConfigValue
	parameterRepository domaincontractsrepository.FirmwareConfigParameter
	node                domainusecasesrepocache.Node
	publisher            domaincontractsnode.Publish
	logger               domaincontractslogger.Leveled
}

func NewUsecaseImpl(
	repository domaincontractsrepository.NodeConfigValue,
	parameterRepository domaincontractsrepository.FirmwareConfigParameter,
	node domainusecasesrepocache.Node,
	publisher domaincontractsnode.Publish,
	logger domaincontractslogger.Leveled,
) domainusecasesnode.ConfigValue {
	return &usecase{
		repository:          repository,
		parameterRepository: parameterRepository,
		node:                 node,
		publisher:            publisher,
		logger:               logger,
	}
}

func (u *usecase) ReadByNodeId(
	ctx context.Context,
	request domainusecasesnode.ReadConfigValuesByNodeIdRequest,
) ([]domainmodels.NodeConfigValue, error) {
	const tag = "node/config_value/ReadByNodeId"

	values, err := u.repository.ReadByNodeId(ctx, request.NodeId)
	if err != nil {
		u.logger.Error(ctx, tag, "failed to read node config values", domainmodels.LoggerMeta{
			"err":     err,
			"node_id": request.NodeId,
		})
		return nil, err
	}

	return values, nil
}

func (u *usecase) SetByNodeId(ctx context.Context, request domainusecasesnode.SetConfigValueRequest) error {
	const tag = "node/config_value/SetByNodeId"

	node, err := u.node.ReadById(ctx, request.NodeId)
	if err != nil {
		u.logger.Error(ctx, tag, "failed to read node", domainmodels.LoggerMeta{
			"err":     err,
			"node_id": request.NodeId,
		})
		return err
	}

	params, err := u.parameterRepository.ReadByFirmwareId(ctx, node.FirmwareId)
	if err != nil {
		u.logger.Error(ctx, tag, "failed to read firmware config parameters", domainmodels.LoggerMeta{
			"err":         err,
			"firmware_id": node.FirmwareId,
		})
		return err
	}

	var valueType string
	found := false
	for _, param := range params {
		if param.Key == request.Key {
			valueType = param.ValueType
			found = true
			break
		}
	}
	if !found {
		return domainmodels.NewError("config key does not exist on the node's current firmware", domainmodels.ErrTypeNotFound, nil)
	}

	if err := validateConfigValue(request.Value, valueType); err != nil {
		return err
	}

	if err := u.repository.Upsert(ctx, request.NodeId, node.FirmwareId, request.Key, request.Value, request.ActorId); err != nil {
		u.logger.Error(ctx, tag, "failed to upsert node config value", domainmodels.LoggerMeta{
			"err":     err,
			"node_id": request.NodeId,
			"key":     request.Key,
		})
		return err
	}

	if err := u.publisher.Config(ctx, node.DeviceId, request.Key, request.Value); err != nil {
		u.logger.Error(ctx, tag, "failed to publish config value", domainmodels.LoggerMeta{
			"err":       err,
			"device_id": node.DeviceId,
			"key":       request.Key,
		})
		return err
	}

	return nil
}

func validateConfigValue(value string, valueType string) error {
	switch valueType {
	case "uint32":
		if _, err := strconv.ParseUint(value, 10, 32); err != nil {
			return domainmodels.NewError("config value must be a valid uint32", domainmodels.ErrTypeValidation, err)
		}
	case "bool":
		if value != "true" && value != "false" {
			return domainmodels.NewError("config value must be \"true\" or \"false\"", domainmodels.ErrTypeValidation, nil)
		}
	case "string":
		// any string value is acceptable
	default:
		return domainmodels.NewError("unrecognized config value_type", domainmodels.ErrTypeFailure, nil)
	}

	return nil
}
```

- [ ] **Step 5: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```
Expected: clean. Nothing constructs these usecases yet (Task 6), so this only proves they compile against the Task 2/3 interfaces.

- [ ] **Step 6: Commit**

```bash
git add internal/domain/usecases/node/config_parameter.go \
        internal/domain/usecases/node/config_value.go \
        internal/application/node/config_parameter \
        internal/application/node/config_value
git commit -m "feat: add config_parameter and config_value application usecases"
```

---

### Task 5: Wire `firmware_management` to ingest a config schema on upload

**Files:**
- Modify: `internal/domain/usecases/node/firmware_management.go`
- Modify: `internal/application/node/firmware_management/usecase.go`

**Interfaces:**
- Consumes: `domainusecasesnode.ConfigParameter` (Task 4).
- Produces: `CreateFirmwareRequest.ConfigSchema`/`ReplaceFirmwareBinaryByIdRequest.ConfigSchema` (both `[]ConfigParameterInput`, optional). Task 6's HTTP handler populates these.

- [ ] **Step 1: Add the `ConfigSchema` field to both request structs**

In `internal/domain/usecases/node/firmware_management.go`, add to `CreateFirmwareRequest` (after `CreatedBy`):
```go
	ConfigSchema []ConfigParameterInput
```
and to `ReplaceFirmwareBinaryByIdRequest` (after `UpdatedBy`):
```go
	ConfigSchema []ConfigParameterInput
```

- [ ] **Step 2: Inject the `ConfigParameter` usecase and call it after a successful upload**

In `internal/application/node/firmware_management/usecase.go`:

Add `configParameter domainusecasesnode.ConfigParameter` to the `usecase` struct and to `NewUsecaseImpl`'s parameters (as the last parameter, after `logger`):
```go
type usecase struct {
	firmware        domainusecasesrepocache.Firmware
	node            domainusecasesrepocache.Node
	storage         domaincontractsstorage.Firmware
	configParameter domainusecasesnode.ConfigParameter
	logger          domaincontractslogger.Leveled
}

func NewUsecaseImpl(
	firmware domainusecasesrepocache.Firmware,
	node domainusecasesrepocache.Node,
	storage domaincontractsstorage.Firmware,
	configParameter domainusecasesnode.ConfigParameter,
	logger domaincontractslogger.Leveled,
) domainusecasesnode.FirmwareManagement {
	return &usecase{
		firmware:        firmware,
		node:            node,
		storage:         storage,
		configParameter: configParameter,
		logger:          logger,
	}
}
```

In `Create`, after the existing `id, err := u.firmware.Create(...)` block succeeds (right before the final `return domainusecasesnode.CreateFirmwareResult{...}, nil`), add:
```go
	if len(request.ConfigSchema) > 0 {
		if err := u.configParameter.ReplaceForFirmware(ctx, domainusecasesnode.ReplaceConfigParametersRequest{
			FirmwareId: id,
			Parameters: request.ConfigSchema,
			ActorId:    request.CreatedBy,
		}); err != nil {
			u.logger.Error(ctx, tag, "failed to ingest firmware config schema", domainmodels.LoggerMeta{
				"err":         err,
				"firmware_id": id,
			})
			return domainusecasesnode.CreateFirmwareResult{}, err
		}
	}
```
(`tag` is already in scope as `const tag = "node/firmware_management/Create"` at the top of the function.)

In `ReplaceBinaryById`, after the existing `u.firmware.UpdateById(...)` call succeeds (right before the final `return domainusecasesnode.FirmwareBinaryStatResult{...}, nil`), add:
```go
	if len(request.ConfigSchema) > 0 {
		if err := u.configParameter.ReplaceForFirmware(ctx, domainusecasesnode.ReplaceConfigParametersRequest{
			FirmwareId: request.Id,
			Parameters: request.ConfigSchema,
			ActorId:    request.UpdatedBy,
		}); err != nil {
			u.logger.Error(ctx, tag, "failed to ingest firmware config schema", domainmodels.LoggerMeta{
				"err":         err,
				"firmware_id": request.Id,
			})
			return domainusecasesnode.FirmwareBinaryStatResult{}, err
		}
	}
```
(`tag` is already in scope as `const tag = "node/firmware_management/ReplaceBinaryById"`.)

If `request.ConfigSchema` is empty/nil in either function, `ReplaceForFirmware` is not called at all — an upload with no schema leaves any existing parameter rows for that firmware untouched (matches the design spec's "no schema change" behavior).

- [ ] **Step 3: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```
Expected: this will fail to compile at the call site in `internal/composition/main/application.go` (`applicationnodefirmwaremanagement.NewUsecaseImpl(firmwareRepoCache, nodeRepoCache, l.infra.firmwareStorage, l.infra.logger)` — 4 args, now needs 5). This is EXPECTED — Task 6 fixes composition wiring. Confirm the ONLY compile error is that one call site (grep for it):
```bash
grep -n "applicationnodefirmwaremanagement.NewUsecaseImpl" internal/composition/main/application.go
```

- [ ] **Step 4: Commit**

```bash
git add internal/domain/usecases/node/firmware_management.go \
        internal/application/node/firmware_management/usecase.go
git commit -m "feat: firmware_management ingests config schema on upload"
```

---

### Task 6: HTTP API — request/response DTOs, handler endpoints, routes, permissions

**Files:**
- Modify: `internal/presentation/http/request/node.go`
- Create: `internal/presentation/http/response/config.go`
- Modify: `internal/presentation/http/handler/node/handler.go`
- Modify: `internal/presentation/http/route/route.go`
- Modify: `database/seeder/permission.json`

**Interfaces:**
- Consumes: `domainusecasesnode.ConfigParameter`/`ConfigValue` (Task 4), `domainusecasesnode.CreateFirmwareRequest.ConfigSchema`/`ReplaceFirmwareBinaryByIdRequest.ConfigSchema` (Task 5).
- Produces: 3 new HTTP endpoints. Task 7 (composition) wires the handler's new constructor params.

- [ ] **Step 1: Add request/response types**

In `internal/presentation/http/request/node.go`, add:
```go
type SetNodeConfigValueRequest struct {
	Key   string `json:"key" example:"mqtt_host"`
	Value string `json:"value" example:"broker.example.com"`
}
```

`internal/presentation/http/response/config.go` (new file):
```go
package presentationhttpresponse

import (
	domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"
)

type FirmwareConfigParameterResponse struct {
	Key       string `json:"key" example:"mqtt_host"`
	ValueType string `json:"value_type" example:"string"`
}

func FirmwareConfigParameters(params []domainmodels.FirmwareConfigParameter) []FirmwareConfigParameterResponse {
	result := make([]FirmwareConfigParameterResponse, 0, len(params))
	for _, param := range params {
		result = append(result, FirmwareConfigParameterResponse{
			Key:       param.Key,
			ValueType: param.ValueType,
		})
	}
	return result
}

type NodeConfigValueResponse struct {
	Key       string `json:"key" example:"mqtt_host"`
	Value     string `json:"value" example:"broker.example.com"`
	UpdatedAt string `json:"updated_at,omitempty" example:"2026-07-29T14:05:00Z"`
}

func NodeConfigValues(values []domainmodels.NodeConfigValue) []NodeConfigValueResponse {
	result := make([]NodeConfigValueResponse, 0, len(values))
	for _, value := range values {
		updatedAt := ""
		if value.UpdatedAt != nil {
			updatedAt = value.UpdatedAt.Format("2006-01-02T15:04:05Z07:00")
		}
		result = append(result, NodeConfigValueResponse{
			Key:       value.Key,
			Value:     value.Value,
			UpdatedAt: updatedAt,
		})
	}
	return result
}
```

- [ ] **Step 2: Add the 3 handler methods**

In `internal/presentation/http/handler/node/handler.go`, add `configParameterUseCase domainusecasesnode.ConfigParameter` and `configValueUseCase domainusecasesnode.ConfigValue` to the `handler` struct and `NewHandler`'s parameters (as the last two, after `otaUseCase`):
```go
type handler struct {
	classUseCase           domainusecasesnode.ClassManagement
	deviceUseCase          domainusecasesnode.DeviceManagement
	firmwareUseCase        domainusecasesnode.FirmwareManagement
	otaUseCase             domainusecasesnode.Ota
	configParameterUseCase domainusecasesnode.ConfigParameter
	configValueUseCase     domainusecasesnode.ConfigValue
}

func NewHandler(
	classUseCase domainusecasesnode.ClassManagement,
	deviceUseCase domainusecasesnode.DeviceManagement,
	firmwareUseCase domainusecasesnode.FirmwareManagement,
	otaUseCase domainusecasesnode.Ota,
	configParameterUseCase domainusecasesnode.ConfigParameter,
	configValueUseCase domainusecasesnode.ConfigValue,
) *handler {
	return &handler{
		classUseCase:           classUseCase,
		deviceUseCase:          deviceUseCase,
		firmwareUseCase:        firmwareUseCase,
		otaUseCase:             otaUseCase,
		configParameterUseCase: configParameterUseCase,
		configValueUseCase:     configValueUseCase,
	}
}
```

Add these 3 handler methods (anywhere after the existing OTA handlers, e.g. right after `otaRequest`):
```go
// FirmwareConfigParametersGet godoc
//
// @Summary Firmware Config Parameters
// @Tags Firmwares
// @Produce json
// @Security BearerAuth
// @Param id path string true "id"
// @Success 200 {array} presentationhttpresponse.FirmwareConfigParameterResponse
// @Failure 400 {object} presentationhttpresponse.ErrorResponse "Invalid Format"
// @Failure 401 {object} presentationhttpresponse.ErrorResponse "Unauthorized"
// @Failure 403 {object} presentationhttpresponse.ErrorResponse "Access Denied"
// @Failure 500 {object} presentationhttpresponse.ErrorResponse "Internal Server Error"
// @Router /v1/firmwares/{id}/config-parameters [get]
func (h *handler) FirmwareConfigParametersGet(c *echo.Context) error {
	firmwareId, err := presentationhttputils.RequiredUUID(c.Param("id"), "id")
	if err != nil {
		return presentationhttputils.Error(c, err, "The firmware ID provided is invalid.")
	}

	params, err := h.configParameterUseCase.ReadByFirmwareId(c.Request().Context(), domainusecasesnode.ReadConfigParametersByFirmwareIdRequest{
		FirmwareId: firmwareId,
	})
	if err != nil {
		return presentationhttputils.Error(c, err, "Unable to read the firmware's config parameters. Please try again.")
	}

	return c.JSON(http.StatusOK, presentationhttpresponse.FirmwareConfigParameters(params))
}

// NodeConfigGet godoc
//
// @Summary Node Config
// @Tags Nodes
// @Produce json
// @Security BearerAuth
// @Param id path string true "id"
// @Success 200 {array} presentationhttpresponse.NodeConfigValueResponse
// @Failure 400 {object} presentationhttpresponse.ErrorResponse "Invalid Format"
// @Failure 401 {object} presentationhttpresponse.ErrorResponse "Unauthorized"
// @Failure 403 {object} presentationhttpresponse.ErrorResponse "Access Denied"
// @Failure 500 {object} presentationhttpresponse.ErrorResponse "Internal Server Error"
// @Router /v1/nodes/{id}/config [get]
func (h *handler) NodeConfigGet(c *echo.Context) error {
	nodeId, err := presentationhttputils.RequiredUUID(c.Param("id"), "id")
	if err != nil {
		return presentationhttputils.Error(c, err, "The node ID provided is invalid.")
	}

	values, err := h.configValueUseCase.ReadByNodeId(c.Request().Context(), domainusecasesnode.ReadConfigValuesByNodeIdRequest{
		NodeId: nodeId,
	})
	if err != nil {
		return presentationhttputils.Error(c, err, "Unable to read the node's config values. Please try again.")
	}

	return c.JSON(http.StatusOK, presentationhttpresponse.NodeConfigValues(values))
}

// NodeConfigPut godoc
//
// @Summary Node Config
// @Tags Nodes
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "id"
// @Param request body presentationhttprequest.SetNodeConfigValueRequest true "request"
// @Success 204
// @Failure 400 {object} presentationhttpresponse.ErrorResponse "Invalid Format"
// @Failure 401 {object} presentationhttpresponse.ErrorResponse "Unauthorized"
// @Failure 403 {object} presentationhttpresponse.ErrorResponse "Access Denied"
// @Failure 404 {object} presentationhttpresponse.ErrorResponse "Not Found"
// @Failure 500 {object} presentationhttpresponse.ErrorResponse "Internal Server Error"
// @Router /v1/nodes/{id}/config [put]
func (h *handler) NodeConfigPut(c *echo.Context) error {
	nodeId, err := presentationhttputils.RequiredUUID(c.Param("id"), "id")
	if err != nil {
		return presentationhttputils.Error(c, err, "The node ID provided is invalid.")
	}

	var req presentationhttprequest.SetNodeConfigValueRequest
	if err := presentationhttputils.Bind(c, &req); err != nil {
		return err
	}

	key, err := presentationhttputils.RequiredString(req.Key, "key")
	if err != nil {
		return presentationhttputils.Error(c, err, "Please provide a config key.")
	}
	value, err := presentationhttputils.RequiredString(req.Value, "value")
	if err != nil {
		return presentationhttputils.Error(c, err, "Please provide a config value.")
	}

	if err := h.configValueUseCase.SetByNodeId(c.Request().Context(), domainusecasesnode.SetConfigValueRequest{
		NodeId:  nodeId,
		Key:     key,
		Value:   value,
		ActorId: presentationhttputils.ActorId(c),
	}); err != nil {
		return presentationhttputils.Error(c, err, "Unable to set the node's config value. Please check your input and try again.")
	}

	return c.NoContent(http.StatusNoContent)
}
```

- [ ] **Step 3: Also pass `config_schema` through on `FirmwarePost` and `FirmwareBinaryPut`**

In `FirmwarePost` (`internal/presentation/http/handler/node/handler.go`), add before the `h.firmwareUseCase.Create(...)` call:
```go
	configSchema, err := parseConfigSchemaFormValue(c.FormValue("config_schema"))
	if err != nil {
		return presentationhttputils.Error(c, err, "The config_schema field must be a valid JSON array of {key,value_type} objects.")
	}
```
and add `ConfigSchema: configSchema,` to the `CreateFirmwareRequest{...}` literal. Add the matching swagger line `// @Param config_schema formData string false "config_schema"` to `FirmwarePost`'s doc comment.

Find the existing `FirmwareBinaryPut` handler (same file, handles `PUT /v1/firmwares/{id}/binary`) and apply the identical pattern: parse `c.FormValue("config_schema")`, add `ConfigSchema: configSchema,` to its `ReplaceFirmwareBinaryByIdRequest{...}` literal, add the matching swagger `@Param` line.

Add this helper function (anywhere in the file, e.g. near `otaRequest`):
```go
func parseConfigSchemaFormValue(raw string) ([]domainusecasesnode.ConfigParameterInput, error) {
	raw = strings.TrimSpace(raw)
	if raw == "" {
		return nil, nil
	}

	var schema []domainusecasesnode.ConfigParameterInput
	if err := json.Unmarshal([]byte(raw), &schema); err != nil {
		return nil, domainmodels.NewError("config_schema must be a valid JSON array", domainmodels.ErrTypeValidation, err)
	}

	return schema, nil
}
```
Add `"encoding/json"`, `"strings"`, and `domainmodels "github.com/MateWorkspace/mate-things/backend/internal/domain/models"` to the file's import block if not already present (check first — `domainmodels` may already be imported elsewhere in this large handler file).

- [ ] **Step 4: Add routes**

In `internal/presentation/http/route/route.go`'s `routeNode` function, add (after the existing `/firmwares/:node_class_id/firmwares` line, before the generic firmware routes, following the existing static-before-generic ordering):
```go
	v1.GET("/firmwares/:id/config-parameters", handler.FirmwareConfigParametersGet, permission("firmware:get"))
```
And, in the `/nodes` section (after `v1.DELETE("/nodes/:id", ...)`):
```go
	v1.GET("/nodes/:id/config", handler.NodeConfigGet, permission("node_config:get"))
	v1.PUT("/nodes/:id/config", handler.NodeConfigPut, permission("node_config:set"))
```

- [ ] **Step 5: Add permissions**

In `database/seeder/permission.json`, add to the `node`/`ota` block (after `"ota:dispatch"`, before the `action` block):
```json
  { "name": "node_config:get", "description": "View a node's config values." },
  { "name": "node_config:set", "description": "Set a node's config value." },
```

- [ ] **Step 6: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```
Expected: fails at the `NewHandler(...)` call site in composition (now needs 6 args, not 4) — EXPECTED, Task 7 fixes it. Confirm:
```bash
grep -rn "presentationhttphandlernode.NewHandler" internal/composition
```

- [ ] **Step 7: Commit**

```bash
git add internal/presentation/http/request/node.go \
        internal/presentation/http/response/config.go \
        internal/presentation/http/handler/node/handler.go \
        internal/presentation/http/route/route.go \
        database/seeder/permission.json
git commit -m "feat: add config HTTP endpoints (firmware config parameters, node config get/set)"
```

---

### Task 7: Composition wiring

**Files:**
- Modify: `internal/composition/main/infrastructure.go`
- Modify: `internal/composition/main/application.go`
- Modify: `internal/composition/main/presentation.go` (or wherever `presentationhttphandlernode.NewHandler` is called — verify the exact file with `grep -rn "presentationhttphandlernode.NewHandler" internal/composition` before editing)

**Interfaces:**
- Consumes: everything from Tasks 1-6.
- Produces: a fully wired, buildable backend with the new endpoints live.

- [ ] **Step 1: Add the two new repositories to `infrastructure.go`**

Add imports:
```go
	infrastructurerepositoryfirmwareconfigparameter "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/firmware_config_parameter"
	infrastructurerepositorynodeconfigvalue "github.com/MateWorkspace/mate-things/backend/internal/infrastructure/repository/node_config_value"
```
Add fields to the `infrastructure` struct (after `firmwareRepository`/near `nodeRepository`):
```go
	firmwareConfigParameterRepository domaincontractsrepository.FirmwareConfigParameter
	nodeConfigValueRepository         domaincontractsrepository.NodeConfigValue
```
Construct them in `newInfrastructure` (after `firmwareRepository`/`nodeRepository` construction):
```go
	firmwareConfigParameterRepository := infrastructurerepositoryfirmwareconfigparameter.NewPostgresImpl(l.drv.dt, &sqrQuestion, &sqrDollar)
	nodeConfigValueRepository := infrastructurerepositorynodeconfigvalue.NewPostgresImpl(l.drv.dt, &sqrQuestion, &sqrDollar)
```
Add them to the `l.infra = &infrastructure{...}` literal (alongside the other repositories):
```go
		firmwareConfigParameterRepository: firmwareConfigParameterRepository,
		nodeConfigValueRepository:         nodeConfigValueRepository,
```

- [ ] **Step 2: Add the two new usecases to `application.go`**

Add imports:
```go
	applicationnodeconfigparameter "github.com/MateWorkspace/mate-things/backend/internal/application/node/config_parameter"
	applicationnodeconfigvalue "github.com/MateWorkspace/mate-things/backend/internal/application/node/config_value"
```
Add fields to the `application` struct (near `nodeFirmwareManagement`):
```go
	nodeConfigParameter domainusecasesnode.ConfigParameter
	nodeConfigValue     domainusecasesnode.ConfigValue
```
Construct `nodeConfigParameter` BEFORE `nodeFirmwareManagement` (since firmware_management now depends on it), and `nodeConfigValue` anywhere after `nodeRepoCache`/`firmwareRepoCache` exist:
```go
	nodeConfigParameter := applicationnodeconfigparameter.NewUsecaseImpl(l.infra.firmwareConfigParameterRepository, l.infra.logger)
	nodeFirmwareManagement := applicationnodefirmwaremanagement.NewUsecaseImpl(
		firmwareRepoCache,
		nodeRepoCache,
		l.infra.firmwareStorage,
		nodeConfigParameter,
		l.infra.logger,
	)
	nodeConfigValue := applicationnodeconfigvalue.NewUsecaseImpl(
		l.infra.nodeConfigValueRepository,
		l.infra.firmwareConfigParameterRepository,
		nodeRepoCache,
		l.infra.nodePublisher,
		l.infra.logger,
	)
```
(Replace the existing `nodeFirmwareManagement := applicationnodefirmwaremanagement.NewUsecaseImpl(...)` call with this 5-arg version — do not leave the old 4-arg call in place.)
Add both to the `l.app = &application{...}` literal:
```go
		nodeConfigParameter: nodeConfigParameter,
		nodeConfigValue:     nodeConfigValue,
```

- [ ] **Step 2: Update the `node` HTTP handler's constructor call**

Find the file constructing `presentationhttphandlernode.NewHandler(...)` (run `grep -rn "presentationhttphandlernode.NewHandler" internal/composition` to find it — likely `internal/composition/main/presentation.go`), and add the two new usecases as the 5th/6th arguments:
```go
presentationhttphandlernode.NewHandler(
    l.app.nodeClassManagement,
    l.app.nodeDeviceManagement,
    l.app.nodeFirmwareManagement,
    l.app.nodeOta,
    l.app.nodeConfigParameter,
    l.app.nodeConfigValue,
)
```
(Match whatever the existing 4-arg call's exact variable names are — they should be `l.app.nodeClassManagement`/`nodeDeviceManagement`/`nodeFirmwareManagement`/`nodeOta` per `application.go`'s struct field names above; adjust only if the actual file uses different local variable names.)

- [ ] **Step 3: Build**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
go build ./...
go vet ./...
```
Expected: clean build — this is the first point where the whole backend compiles together (Tasks 5/6 each left one call site broken on purpose; this task fixes both).

- [ ] **Step 4: Regenerate swagger docs**

```bash
cd /home/dodol/Repositories/mate/mate-things/backend
swag init -g cmd/main/main.go -o docs/swagger --parseInternal --parseDependency
```
Expected: no errors; `docs/swagger/swagger.json` includes the 3 new endpoints.

- [ ] **Step 5: End-to-end verification**

```bash
cd /home/dodol/Repositories/mate/mate-things
docker compose up --build -d
migrate -path backend/database/migrations -database "$DATABASE_URL" up
```
Then, authenticated as an admin (per this repo's existing auth flow):
1. Upload a firmware via `curl -F node_class_id=... -F name=test-fw -F file=@firmware.bin -F 'config_schema=[{"key":"mqtt_proto","value_type":"string"}]' .../v1/firmwares` — confirm `GET /v1/firmwares/{id}/config-parameters` returns `[{"key":"mqtt_proto","value_type":"string"}]`.
2. `PUT /v1/nodes/{id}/config` with `{"key":"mqtt_proto","value":"mqtts"}` on a node running that firmware — confirm 204, then `GET /v1/nodes/{id}/config` reflects it.
3. `PUT /v1/nodes/{id}/config` with an unknown key — confirm 404.
4. Subscribe a `paho-mqtt` test client to `/sub/{device_id}/config` before step 2 and confirm it receives `{"key":"mqtt_proto","value":"mqtts"}`.

- [ ] **Step 6: Commit**

```bash
git add internal/composition/main/infrastructure.go \
        internal/composition/main/application.go \
        internal/composition/main/presentation.go \
        docs/swagger
git commit -m "feat: wire config_parameter/config_value into composition"
```
(Adjust the `presentation.go` path in the `git add` if Step 2 found a different actual filename.)

---

### Task 8: `upload.py` automatic schema parsing

**Files (repo: `/home/dodol/Repositories/mate/mate-espidf-base`):**
- Modify: `upload.py`

**Interfaces:**
- Produces: `parse_preloaded_schema(repo_root: Path) -> list[dict]`; `create_firmware`/`replace_firmware_binary` both send a `config_schema` form field.

- [ ] **Step 1: Write the schema parser**

Add to `upload.py` (after `load_config`, before `login`):
```python
import re

_SCHEMA_ROW_RE = re.compile(
    r"X\(\s*\w+\s*,\s*(DOMAIN_MODELS_PRELOADED_\w+_KEY)\s*,\s*DOMAIN_MODELS_PRELOADED_VALUE_TYPE_(\w+)\s*\)"
)
_KEY_DEFINE_RE = re.compile(
    r'#define\s+(DOMAIN_MODELS_PRELOADED_\w+_KEY)\s+"([^"]+)"'
)
_TYPE_MAP = {"STRING": "string", "UINT32": "uint32", "BOOL": "bool"}


def parse_preloaded_schema(repo_root: Path) -> list[dict]:
    header_path = repo_root / "main" / "include" / "domain" / "models" / "preloaded.h"
    text = header_path.read_text()

    key_defines = dict(_KEY_DEFINE_RE.findall(text))

    schema = []
    for key_macro, type_suffix in _SCHEMA_ROW_RE.findall(text):
        if key_macro not in key_defines:
            raise SystemExit(
                f"preloaded.h schema row references undefined key macro '{key_macro}' - "
                "check that DOMAIN_MODELS_PRELOADED_SCHEMA(X) and the _KEY defines are in sync"
            )
        if type_suffix not in _TYPE_MAP:
            raise SystemExit(f"preloaded.h schema row has unrecognized value type suffix '{type_suffix}'")

        schema.append({"key": key_defines[key_macro], "type": _TYPE_MAP[type_suffix]})

    if not schema:
        raise SystemExit(
            "no config schema entries found in preloaded.h - "
            "check DOMAIN_MODELS_PRELOADED_SCHEMA(X) is present and well-formed"
        )

    return schema
```

- [ ] **Step 2: Send the parsed schema with both upload calls**

Modify `create_firmware` to accept and send it:
```python
def create_firmware(base_url: str, token: str, node_class_id: str, name: str, file_path: Path, config_schema: list[dict]) -> dict:
    with file_path.open("rb") as f:
        resp = requests.post(
            f"{base_url}/v1/firmwares",
            headers={"Authorization": f"Bearer {token}"},
            data={
                "node_class_id": node_class_id,
                "name": name,
                "config_schema": json.dumps(config_schema),
            },
            files={"file": (file_path.name, f, "application/octet-stream")},
            timeout=60,
        )
    resp.raise_for_status()
    return resp.json()
```

Modify `replace_firmware_binary` similarly:
```python
def replace_firmware_binary(base_url: str, token: str, firmware_id: str, file_path: Path, config_schema: list[dict]) -> dict:
    with file_path.open("rb") as f:
        resp = requests.put(
            f"{base_url}/v1/firmwares/{firmware_id}/binary",
            headers={"Authorization": f"Bearer {token}"},
            data={"config_schema": json.dumps(config_schema)},
            files={"file": (file_path.name, f, "application/octet-stream")},
            timeout=60,
        )
    resp.raise_for_status()
    return resp.json()
```

In `main()`, compute the schema once (right after `file_path` is resolved and validated) and pass it to whichever branch runs:
```python
    config_schema = parse_preloaded_schema(Path(__file__).parent)
    print(f"Parsed {len(config_schema)} config parameter(s) from preloaded.h")
```
Update the two call sites:
```python
        result = replace_firmware_binary(base_url, token, existing_id, file_path, config_schema)
```
```python
        result = create_firmware(base_url, token, node_class_id, firmware_name, file_path, config_schema)
```

- [ ] **Step 3: Verify**

```bash
cd /home/dodol/Repositories/mate/mate-espidf-base
python3 -c "
from pathlib import Path
import sys
sys.path.insert(0, '.')
from upload import parse_preloaded_schema
import json
print(json.dumps(parse_preloaded_schema(Path('.')), indent=2))
"
```
Expected output (order matches `DOMAIN_MODELS_PRELOADED_SCHEMA(X)`'s row order):
```json
[
  {"key": "mqtt_proto", "type": "string"},
  {"key": "mqtt_host", "type": "string"},
  {"key": "mqtt_port", "type": "string"},
  {"key": "mqtt_user", "type": "string"},
  {"key": "mqtt_pass", "type": "string"},
  {"key": "sys_rst_aft_ms", "type": "uint32"},
  {"key": "wifi_try_init", "type": "bool"}
]
```
Then run a real upload against a running backend (from Task 7's Step 5 environment) and confirm it completes without error and the backend's `GET /v1/firmwares/{id}/config-parameters` shows these same 7 entries.

- [ ] **Step 4: Commit**

```bash
git add upload.py
git commit -m "feat: upload.py automatically parses and sends preloaded.h's config schema"
```

---

## Firmware tasks (repo: `/home/dodol/Repositories/mate/mate-espidf-base`)

### Task 9: `def_sub` contract + infra impls gain a `config` subscription

**Files:**
- Modify: `main/include/domain/contracts/messaging/def_sub.h`
- Modify: `main/src/infrastructure/messaging/def_sub/mqtt_impl.c`
- Modify: `main/include/infrastructure/messaging/def_sub/stub_impl_types.h`
- Modify: `main/src/infrastructure/messaging/def_sub/stub_impl.c`
- Modify: `main/include/infrastructure/messaging/def_sub/stub_impl_utils.h`
- Modify: `main/src/infrastructure/messaging/def_sub/stub_impl_utils.c`

**Interfaces:**
- Produces: `dom_contracts_messaging_def_sub_t.config(device_id, self)`. Task 10 (messaging_callbacks) consumes it.

- [ ] **Step 1: Add `config` to the contract**

In `main/include/domain/contracts/messaging/def_sub.h`, add to the struct (after `action`):
```c
    dom_models_error_t (*config)(
        const char*                        device_id,
        dom_contracts_messaging_def_sub_t* self
    );
```

- [ ] **Step 2: Implement it in `mqtt_impl.c`**

In `main/src/infrastructure/messaging/def_sub/mqtt_impl.c`, add a prototype (after `action_impl`'s):
```c
static dom_models_error_t config_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
);
```
Wire it in `inf_messaging_def_sub_mqtt_impl_new` (after `self->action = action_impl;`):
```c
    self->config = config_impl;
```
Implement it (after `action_impl`'s definition):
```c
static dom_models_error_t config_impl(
    const char*                        device_id,
    dom_contracts_messaging_def_sub_t* self
) {
    if (!self || !self->ctx) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return inf_messaging_def_sub_mqtt_impl_subscribe_suffix(self->ctx, device_id, "config");
}
```

- [ ] **Step 3: Add the matching `stub_impl` fields/functions**

In `main/include/infrastructure/messaging/def_sub/stub_impl_types.h`, add `bool config_subscribed;` to `inf_messaging_def_sub_stub_impl_cfg_t`, `.config_subscribed = false,` to `INF_MESSAGING_DEF_SUB_STUB_IMPL_CFG_DEFAULT()`, and to `inf_messaging_def_sub_stub_impl_ctx_t`: `bool config_subscribed;`, `char last_config_device_id[INF_MESSAGING_DEF_SUB_STUB_IMPL_DEVICE_ID_MAX_LEN];`, `size_t config_subscribe_cnt;` — each placed alongside their `action`-suffixed siblings.

In `main/include/infrastructure/messaging/def_sub/stub_impl_utils.h`, add the prototype:
```c
dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_config(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
);
```

In `main/src/infrastructure/messaging/def_sub/stub_impl_utils.c`:
- In `inf_messaging_def_sub_stub_impl_load_cfg`, add `ctx->config_subscribed = cfg->config_subscribed;`.
- Add the function (after `inf_messaging_def_sub_stub_impl_subscribe_action`):
```c
dom_models_error_t inf_messaging_def_sub_stub_impl_subscribe_config(
    inf_messaging_def_sub_stub_impl_ctx_t* ctx,
    const char*                            device_id
) {
    if (!ctx || !cstr_available(device_id)) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    ctx->config_subscribed = true;
    copy_cstr(ctx->last_config_device_id, sizeof(ctx->last_config_device_id), device_id);
    ctx->config_subscribe_cnt++;

    return DOMAIN_MODELS_ERROR_OK;
}
```

In `main/src/infrastructure/messaging/def_sub/stub_impl.c`: add a `config_impl` prototype/wiring/definition mirroring `action_impl` exactly (calls `inf_messaging_def_sub_stub_impl_subscribe_config(self->ctx, device_id)`), and add `self->config = config_impl;` in `inf_messaging_def_sub_stub_impl_new`.

- [ ] **Step 4: Build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```
Expected: clean (no new source files this task, only additions to existing ones).

- [ ] **Step 5: Commit**

```bash
git add main/include/domain/contracts/messaging/def_sub.h \
        main/src/infrastructure/messaging/def_sub/mqtt_impl.c \
        main/include/infrastructure/messaging/def_sub/stub_impl_types.h \
        main/src/infrastructure/messaging/def_sub/stub_impl.c \
        main/include/infrastructure/messaging/def_sub/stub_impl_utils.h \
        main/src/infrastructure/messaging/def_sub/stub_impl_utils.c
git commit -m "feat: add config subscription to the def_sub messaging contract"
```

---

### Task 10: `messaging_callbacks` subscribes to `config` on connect

**Files:**
- Modify: `main/src/application/internal/messaging_callbacks/impl.c`

**Interfaces:**
- Consumes: `dom_contracts_messaging_def_sub_t.config` (Task 9).

- [ ] **Step 1: Add the subscribe call**

In `subscribe_defaults_impl` (in `main/src/application/internal/messaging_callbacks/impl.c`), add, after the existing `action` subscribe block and before the final success log:
```c
    err = ctx->cfg.def_sub->config(device_id_str, ctx->cfg.def_sub);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->cfg.logger->error(ctx->cfg.logger, tag, "Failed to subscribe to config: %s (%d)", dom_models_error_str(err), (int)err);
        return err;
    }
```

- [ ] **Step 2: Build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```

- [ ] **Step 3: Commit**

```bash
git add main/src/application/internal/messaging_callbacks/impl.c
git commit -m "feat: subscribe to the config MQTT topic on connect"
```

---

### Task 11: MQTT context gains a `config_topic` and `settings` usecase reference

**Files:**
- Modify: `main/include/presentation/mqtt/context.h`
- Modify: `main/src/presentation/mqtt/context.c`
- Modify: `main/include/presentation/mqtt/context_utils.h`
- Modify: `main/src/presentation/mqtt/context_utils.c`
- Modify: `main/src/composition/main/presentation.c`

**Interfaces:**
- Consumes: `dom_usecases_internal_settings_t*` (existing usecase, already available as `launcher->application.settings`).
- Produces: `pres_mqtt_context_t.config_topic`/`.settings`; `pres_mqtt_context_new`'s signature gains a `settings` parameter (now 5 params). Task 12 (new config handler) consumes `ctx->settings`/`ctx->config_topic`.

- [ ] **Step 1: Add the fields to `pres_mqtt_context_t` and the `settings` param to `_new`**

In `main/include/presentation/mqtt/context.h`, add `#include "domain/usecases/internal/settings.h"`. Add to the struct (after `ota`):
```c
    dom_usecases_internal_settings_t*            settings;
```
and after `action_topic`:
```c
    char                                         config_topic[PRES_MQTT_CONTEXT_TOPIC_MAX_LEN];
```
Change `pres_mqtt_context_new`'s signature to add `settings` as the 4th parameter (before `ota`, matching this file's existing param ordering convention where `messaging_callbacks` precedes `ota`):
```c
pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
);
```

- [ ] **Step 2: Update `context_utils.h`/`.c`'s `validate_cfg` signature**

In `main/include/presentation/mqtt/context_utils.h`, add `#include "domain/usecases/internal/settings.h"` and update the prototype to match the new param order/count:
```c
dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
);
```
In `main/src/presentation/mqtt/context_utils.c`, update the implementation to match:
```c
dom_models_error_t pres_mqtt_context_validate_cfg(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
) {
    if (!logger || !preloaded_repository || !messaging_callbacks || !settings || !ota) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 3: Update `context.c`**

In `main/src/presentation/mqtt/context.c`'s `pres_mqtt_context_new`, update the signature to match, update the `validate_cfg` call site to pass `settings` in the new position, and add `self->settings = settings;` (alongside `self->ota = ota;`):
```c
pres_mqtt_context_t* pres_mqtt_context_new(
    dom_contracts_logger_leveled_t*              logger,
    dom_contracts_repository_preloaded_t*        preloaded_repository,
    dom_usecases_internal_messaging_callbacks_t* messaging_callbacks,
    dom_usecases_internal_settings_t*            settings,
    dom_usecases_internal_ota_t*                 ota
) {
    if (pres_mqtt_context_validate_cfg(logger, preloaded_repository, messaging_callbacks, settings, ota) != DOMAIN_MODELS_ERROR_OK) {
        return NULL;
    }

    pres_mqtt_context_t* self = (pres_mqtt_context_t*)calloc(1, sizeof(pres_mqtt_context_t));
    if (!self) {
        return NULL;
    }

    self->logger               = logger;
    self->preloaded_repository = preloaded_repository;
    self->messaging_callbacks  = messaging_callbacks;
    self->settings             = settings;
    self->ota                  = ota;
    self->mqtt_client          = NULL;
    /* (rest of the function unchanged) */
```
In `pres_mqtt_context_init`, add, after the existing `action_topic` `snprintf` block:
```c
    written = snprintf(self->config_topic, sizeof(self->config_topic), "/sub/%s/config", self->device_id_str);
    if (written <= 0 || (size_t)written >= sizeof(self->config_topic)) {
        self->logger->error(self->logger, tag, "Failed to build config topic string");
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }
```

- [ ] **Step 4: Update the call site in `composition/main/presentation.c`**

Change:
```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.ota
    );
```
to:
```c
    launcher->presentation.mqtt_context = pres_mqtt_context_new(
        launcher->infrastructure.logger,
        launcher->infrastructure.preloaded_repository,
        launcher->application.messaging_callbacks,
        launcher->application.settings,
        launcher->application.ota
    );
```

- [ ] **Step 5: Build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py build
```
Expected: clean (no new files).

- [ ] **Step 6: Commit**

```bash
git add main/include/presentation/mqtt/context.h \
        main/src/presentation/mqtt/context.c \
        main/include/presentation/mqtt/context_utils.h \
        main/src/presentation/mqtt/context_utils.c \
        main/src/composition/main/presentation.c
git commit -m "feat: mqtt context gains config_topic and a settings usecase reference"
```

---

### Task 12: New `presentation/mqtt/handler/config` module

**Files:**
- Create: `main/include/presentation/mqtt/handler/config/dto.h`
- Create: `main/include/presentation/mqtt/handler/config/handler.h`
- Create: `main/src/presentation/mqtt/handler/config/dto.c`
- Create: `main/src/presentation/mqtt/handler/config/handler.c`
- Modify: `main/src/presentation/mqtt/event/on_message.c`

**Interfaces:**
- Consumes: `ctx->settings` (Task 11), `ctx->config_topic` (Task 11), `dom_models_preloaded_schema`/`DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT` (from sub-project 1, already merged in `domain/models/preloaded.h`).
- Produces: `pres_mqtt_handler_config(ctx, data, data_len)`.

- [ ] **Step 1: Write `dto.h`/`dto.c`**

`main/include/presentation/mqtt/handler/config/dto.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H
#define PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H

#include <stdbool.h>

#include "domain/models/error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRES_MQTT_HANDLER_CONFIG_DTO_KEY_MAX_LEN   32
#define PRES_MQTT_HANDLER_CONFIG_DTO_VALUE_MAX_LEN 128

typedef struct {
    char key[PRES_MQTT_HANDLER_CONFIG_DTO_KEY_MAX_LEN];
    bool key_set;
    char value[PRES_MQTT_HANDLER_CONFIG_DTO_VALUE_MAX_LEN];
    bool value_set;
} pres_mqtt_handler_config_dto_request_t;

/* Parses the /sub/<device_id>/config payload's key/value as raw strings -
   type-specific interpretation (uint32/bool parsing) happens in the
   handler, using the preloaded config schema to look up each key's type,
   not here (keeps this DTO layer a pure JSON-shape concern, matching
   every other handler's dto.c in this codebase). */
dom_models_error_t pres_mqtt_handler_config_dto_decode(
    const char*                              data,
    int                                       data_len,
    pres_mqtt_handler_config_dto_request_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_CONFIG_DTO_H */
```

`main/src/presentation/mqtt/handler/config/dto.c`:
```c
#include "presentation/mqtt/handler/config/dto.h"

#include <string.h>

#include "cJSON.h"

dom_models_error_t pres_mqtt_handler_config_dto_decode(
    const char*                              data,
    int                                       data_len,
    pres_mqtt_handler_config_dto_request_t* out
) {
    if (!data || data_len <= 0 || !out) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    cJSON* json = cJSON_ParseWithLength(data, (size_t)data_len);
    if (!json) {
        return DOMAIN_MODELS_ERROR_BAD_ARGUMENT;
    }

    cJSON* key_item   = cJSON_GetObjectItemCaseSensitive(json, "key");
    cJSON* value_item = cJSON_GetObjectItemCaseSensitive(json, "value");

    if (cJSON_IsString(key_item) && key_item->valuestring) {
        strncpy(out->key, key_item->valuestring, sizeof(out->key) - 1);
        out->key_set = true;
    }

    if (cJSON_IsString(value_item) && value_item->valuestring) {
        strncpy(out->value, value_item->valuestring, sizeof(out->value) - 1);
        out->value_set = true;
    }

    cJSON_Delete(json);

    return DOMAIN_MODELS_ERROR_OK;
}
```

- [ ] **Step 2: Write `handler.h`/`handler.c`**

`main/include/presentation/mqtt/handler/config/handler.h`:
```c
#ifndef PRESENTATION_MQTT_HANDLER_CONFIG_HANDLER_H
#define PRESENTATION_MQTT_HANDLER_CONFIG_HANDLER_H

#include "presentation/mqtt/context.h"

#ifdef __cplusplus
extern "C" {
#endif

void pres_mqtt_handler_config(pres_mqtt_context_t* ctx, const char* data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* PRESENTATION_MQTT_HANDLER_CONFIG_HANDLER_H */
```

`main/src/presentation/mqtt/handler/config/handler.c`:
```c
#include "presentation/mqtt/handler/config/handler.h"

#include <stdlib.h>
#include <string.h>

#include "domain/models/error.h"
#include "domain/models/preloaded.h"
#include "presentation/mqtt/handler/config/dto.h"

#define BASE_TAG "pres_mqtt_config"

void pres_mqtt_handler_config(pres_mqtt_context_t* ctx, const char* data, int data_len) {
    const char* tag = BASE_TAG "/handle";

    ctx->logger->info(ctx->logger, tag, "Received config request via MQTT");

    pres_mqtt_handler_config_dto_request_t request;
    dom_models_error_t                     err = pres_mqtt_handler_config_dto_decode(data, data_len, &request);
    if (err != DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->error(ctx->logger, tag, "Failed to parse config payload: %s (%d)", dom_models_error_str(err), (int)err);
        return;
    }

    if (!request.key_set || !request.value_set) {
        ctx->logger->error(ctx->logger, tag, "Invalid config payload: missing key or value field");
        return;
    }

    const dom_models_preloaded_schema_entry_t* matched_entry = NULL;
    for (size_t i = 0; i < DOMAIN_MODELS_PRELOADED_SCHEMA_COUNT; i++) {
        if (strcmp(dom_models_preloaded_schema[i].key, request.key) == 0) {
            matched_entry = &dom_models_preloaded_schema[i];
            break;
        }
    }
    if (!matched_entry) {
        ctx->logger->warn(ctx->logger, tag, "Unknown config key: %s", request.key);
        return;
    }

    dom_usecases_internal_settings_preloaded_update_t update;
    memset(&update, 0, sizeof(update));

    if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PROTO_KEY) == 0) {
        strncpy(update.mqtt_proto, request.value, sizeof(update.mqtt_proto) - 1);
        update.mqtt_proto_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_HOST_KEY) == 0) {
        strncpy(update.mqtt_host, request.value, sizeof(update.mqtt_host) - 1);
        update.mqtt_host_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PORT_KEY) == 0) {
        strncpy(update.mqtt_port, request.value, sizeof(update.mqtt_port) - 1);
        update.mqtt_port_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_USER_KEY) == 0) {
        strncpy(update.mqtt_user, request.value, sizeof(update.mqtt_user) - 1);
        update.mqtt_user_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_MQTT_PASS_KEY) == 0) {
        strncpy(update.mqtt_pass, request.value, sizeof(update.mqtt_pass) - 1);
        update.mqtt_pass_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_SYSTEM_RESTART_AFTER_MS_KEY) == 0) {
        char* endptr        = NULL;
        unsigned long value = strtoul(request.value, &endptr, 10);
        if (!endptr || *endptr != '\0') {
            ctx->logger->warn(ctx->logger, tag, "Invalid config value for %s: %s", request.key, request.value);
            return;
        }
        update.system_restart_after_ms     = (uint32_t)value;
        update.system_restart_after_ms_set = true;
    } else if (strcmp(request.key, DOMAIN_MODELS_PRELOADED_WIFI_STA_TRY_CONNECT_ON_INIT_KEY) == 0) {
        if (strcmp(request.value, "true") != 0 && strcmp(request.value, "false") != 0) {
            ctx->logger->warn(ctx->logger, tag, "Invalid config value for %s: %s", request.key, request.value);
            return;
        }
        /* dom_usecases_internal_settings_preloaded_update_t has no
           wifi_sta_try_connect_on_init field today (the settings usecase
           only updates mqtt_*/system_restart_after_ms) - this key exists
           in the schema for BLE/read visibility but is not yet a settings
           usecase update path. Log and skip rather than silently no-op. */
        ctx->logger->warn(ctx->logger, tag, "Config key %s is read-only via MQTT (not yet supported by the settings usecase)", request.key);
        return;
    }

    bool               restart_required = false;
    dom_models_error_t set_err          = ctx->settings->set_preloaded(ctx->settings, &update, &restart_required);
    if (set_err == DOMAIN_MODELS_ERROR_OK) {
        ctx->logger->info(ctx->logger, tag, "Config value updated successfully via MQTT: %s", request.key);
    }
    /* No error log on failure here - the usecase (settings' set_preloaded_impl)
       already logs the failure; re-logging the same condition here would
       double-log it (matches the action handler's restart-path precedent). */
}
```

Note on `wifi_sta_try_connect_on_init`: cross-checking `domain/usecases/internal/settings.h`'s `dom_usecases_internal_settings_preloaded_update_t` (sub-project 1 context) confirms it has no field for this key — the settings usecase's `set_preloaded` only ever updates the 5 mqtt_* strings and `system_restart_after_ms`. Rather than silently dropping the value, the handler explicitly warns and returns for this one key. This is a real, narrow gap worth flagging to the final reviewer/human: closing it would mean extending `dom_usecases_internal_settings_preloaded_update_t` and its `set_preloaded_impl`, which is out of this plan's stated scope (Task 3's design didn't call for touching the settings update struct) — call it out explicitly rather than silently implementing a partial workaround.

- [ ] **Step 3: Wire the new topic into `on_message.c`**

In `main/src/presentation/mqtt/event/on_message.c`, add `#include "presentation/mqtt/handler/config/handler.h"` and one more `else if` branch (after the `action_topic` branch, before the final `else`):
```c
    } else if (strcmp(ctx->topic_scratch, ctx->config_topic) == 0) {
        pres_mqtt_handler_config(ctx, event->data, event->data_len);
    } else {
```

- [ ] **Step 4: Reconfigure and build**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py reconfigure
idf.py build
```

- [ ] **Step 5: Commit**

```bash
git add main/include/presentation/mqtt/handler/config \
        main/src/presentation/mqtt/handler/config \
        main/src/presentation/mqtt/event/on_message.c
git commit -m "feat: add MQTT config handler mapping key/value updates onto settings"
```

---

### Task 13: Flash, hardware verification, and docs

**Files:**
- Modify: `docs/agent_test/v1.0.0-dev.1/scenario/03-mqtt-registration-status.md` (or wherever the MQTT scenario docs live — verify the exact file with the existing action/ota scenario coverage before editing; add a new scenario file `docs/agent_test/v1.0.0-dev.1/scenario/11-mqtt-config.md` if no existing file fits, following the same structure as the BLE GATT services scenario doc from sub-project 1)

**Interfaces:** none (verification + documentation task).

- [ ] **Step 1: Flash and verify boot**

```bash
source "/home/dodol/.espressif/tools/activate_idf_v6.0.2.sh"
cd /home/dodol/Repositories/mate/mate-espidf-base
idf.py -p /dev/ttyACM0 flash
```
Watch the boot log briefly (same approach as prior sessions' hardware checks) for a clean boot, no crash, and a log line confirming the `config` subscription succeeded.

- [ ] **Step 2: Hardware-in-loop MQTT verification**

Using `paho-mqtt` (Python) directly against the real broker (same credentials/host as prior MQTT scenarios this session):
1. Publish `{"key":"mqtt_proto","value":"mqtts"}` to `/sub/<device_id>/config` — confirm (via BLE settings read, or a subsequent MQTT registration) the device's `mqtt_proto` actually changed.
2. Publish `{"key":"nonexistent_key","value":"x"}` — confirm the device logs "Unknown config key" and does not crash.
3. Publish `{"key":"sys_rst_aft_ms","value":"not_a_number"}` — confirm the device logs "Invalid config value" and does not crash.
4. Publish `{"key":"wifi_try_init","value":"true"}` — confirm the device logs the "read-only via MQTT" warning and does not crash.

- [ ] **Step 3: Update test docs**

Add a new scenario doc (or extend an existing MQTT one) documenting the `/sub/{device_id}/config` topic's payload shape and the 4 verification results above, following the existing scenario doc format (numbered checklist items, `[x]`/`[ ]` results).

- [ ] **Step 4: Commit**

```bash
git add docs/agent_test
git commit -m "docs: add MQTT config topic hardware verification results"
```
