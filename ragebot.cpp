// ============================================================================
//  ragebot.cpp
//  rage bot
// ============================================================================
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstddef>
#include <cstdlib>

// ============================================================================
//  базовые типы
// ============================================================================
struct Vector
{
    float x, y, z;

    Vector() : x(0.f), y(0.f), z(0.f) {}
    Vector(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector operator+(const Vector& v) const { return Vector(x + v.x, y + v.y, z + v.z); }
    Vector operator-(const Vector& v) const { return Vector(x - v.x, y - v.y, z - v.z); }
    Vector operator*(float s) const        { return Vector(x * s, y * s, z * s); }

    float Dot(const Vector& v) const { return x * v.x + y * v.y + z * v.z; }

    Vector Cross(const Vector& v) const
    {
        return Vector(y * v.z - z * v.y,
                      z * v.x - x * v.z,
                      x * v.y - y * v.x);
    }

    float LengthSqr() const { return x * x + y * y + z * z; }
    float Length() const    { return sqrtf(LengthSqr()); }

    Vector Normalized() const
    {
        const float len = Length();
        if (len < 1e-17f || len > 1e17f)
            return Vector();

        return *this * (1.f / len);
    }
};

// ============================================================================
//  проверка указателей
// ============================================================================
template <class T>
static inline bool IsValid(const T* p)
{
    return ((uintptr_t)p - 0x10000) <= 0x7FFFFFFEFFFEull;
}

static inline bool IsValidAddr(uintptr_t address)
{
    return (address - 0x10000) <= 0x7FFFFFFEFFFEull;
}

float FastSqrt(float value);

// ============================================================================
//  игровые структуры
// ============================================================================
struct CUserCmd
{
    char     pad0[8];
    int      command_number;
    char     pad1[0x64];
    uint64_t buttons;
};

struct LocalData
{
    Vector   origin;            // 0x000
    char     pad0[0x18];        // 0x00C
    Vector   viewOffset;        // 0x024
    char     pad1[0x38];        // 0x030
    uint8_t  flags;             // 0x068
    char     pad2[0x3B];        // 0x069
    int      ammo;              // 0x0A4
    char     pad3[0x78];        // 0x0A8
    int      tickCount;         // 0x120
    char     pad4[0x2C];        // 0x124
    void*    pawn;              // 0x150
    char     pad5[0x04];        // 0x158
    int      tick;              // 0x15C
    char     pad6[0x04];        // 0x160
    float    tickTime;          // 0x164
    char     pad7[0x10];        // 0x168
    uint64_t buttons;           // 0x178
    char     pad8[0x104];       // 0x180
    int      lagRecord;         // 0x284
    char     pad9[0xCC];        // 0x288
    int      tickA;             // 0x354
    float    fracA;             // 0x358
    int      tickB;             // 0x35C
    float    fracB;             // 0x360
    int      tickC;             // 0x364
    float    fracC;             // 0x368
    char     pad10[0x08];       // 0x36C
    int      clip;              // 0x374
    char     pad11[0x430];      // 0x378
    void*    weapon;            // 0x7A8
    void*    weaponData;        // 0x7B0
    uint16_t weaponId;          // 0x7B8
};

static_assert(sizeof(LocalData) == 0x7C0, "LocalData layout");
static_assert(offsetof(LocalData, flags)     == 0x068, "flags");
static_assert(offsetof(LocalData, ammo)      == 0x0A4, "ammo");
static_assert(offsetof(LocalData, tickCount) == 0x120, "tickCount");
static_assert(offsetof(LocalData, pawn)      == 0x150, "pawn");
static_assert(offsetof(LocalData, tick)      == 0x15C, "tick");
static_assert(offsetof(LocalData, tickTime)  == 0x164, "tickTime");
static_assert(offsetof(LocalData, buttons)   == 0x178, "buttons");
static_assert(offsetof(LocalData, lagRecord) == 0x284, "lagRecord");
static_assert(offsetof(LocalData, clip)      == 0x374, "clip");
static_assert(offsetof(LocalData, weapon)    == 0x7A8, "weapon");
static_assert(offsetof(LocalData, weaponId)  == 0x7B8, "weaponId");

struct CmdResult
{
    uint8_t  active;
    uint8_t  canShoot;
    uint8_t  wordHi;         // +2  верхний байт слова флага "можно стрелять"
    uint8_t  compareA;       // +3  флаг сравнения "позже/раньше"
    uint8_t  compareB;       // +4  второй флаг сравнения
    uint8_t  slotFlag;       // +5  флаг слота (из команды)
    uint8_t  gateFlag;       // +6  глобальный флаг допуска
    uint8_t  bothFlag;       // +7  флаг "оба условия"
    uint8_t  field08;        // +8  байт, используемый снапшотом как условие
    uint8_t  field09;
    uint8_t  field0A;
    char     pad1[1];
    int32_t  field0C;        // +0x0C
    int32_t  field10;        // +0x10  дистанция (биты float)
    int32_t  field14;        // +0x14  дистанция * 64
    float    refineStep;     // +0x18  шаг уточнения
    float    angleX;         // +0x1C
    float    angleY;         // +0x20
    float    angleZ;         // +0x24
    float    forwardX;       // +0x28
    float    forwardY;       // +0x2C
    float    forwardZ;       // +0x30
    float    originXY[2];    // +0x34  origin + viewOffset по x, y
    float    height;         // +0x3C
    float    baseXY[2];      // +0x40  origin по x, y
    int32_t  field48;        // +0x48
    int32_t  tick;           // +0x4C
    char     pad5[4];        // +0x50
    uint8_t  resolved;       // +0x54
};

struct RageContext
{
    uint8_t  valid;
    uint8_t  limiters;
    char     pad0[2];
    int32_t  limiterFlags;
    uint8_t  doubleTap;
    uint8_t  silent;
    char     pad1[0x7C6];
    int32_t  ring0;
    int32_t  ring1;
    int32_t  ring2;
    int32_t  field7DC;
    char     pad2[0x64];
    int32_t  lastTick;
    float    lastTime;
};

struct RageSettings
{
    int      group;
    int      history;
    uint8_t  doubleTap;
    uint8_t  automaticFire;
    char     pad0[2];
    float    spread;
    float    minDamage;
    float    hitchance;
    float    forceShotHitchance;
    float    extrapolation;
    int      refineStep;
    uint8_t  limiters;
    char     pad1[3];
    float    spreadLimited;
    float    aimSpeed;
    float    reactionTime;
    float    killDelay;
    int      backtrack;
    int      maxWalls;
    uint8_t  selftrack;
    char     pad2[3];
    int      limiterExtra;
    int      spreadGrew;
    float    requiredHitchance;     // 0x4C  выбранный порог (force_shot либо базовый)
    uint8_t  forceShot;
    uint8_t  autoStop;
    uint8_t  autoScope;
    uint8_t  stopInAir;
    uint8_t  stopOnLand;
    uint8_t  stopBetweenShots;
    uint8_t  duckPeek;
    char     pad4[1];
    int      futureTicks;
    uint8_t  delayShot;
    uint8_t  delayShotDamage;
    char     pad5[2];
    int      removeSpreadMode;
    int      hitboxMask;
    uint8_t  doublePrecision;
};

struct CmdRecord
{
    int      command;
    int      count;
    char     data[0x74];
    uint8_t  flags;
    char     pad0[0xB7];
    int      lagIndex;
    char     pad1[0x24];
    uint64_t predId;
};

struct ShotPos
{
    float    angle;
    char     pad0[4];
    uint16_t flags;
    char     pad1[0x3E];
};

struct ShotSlot
{
    int   index;
    float frac;
};

struct ShotMarker
{
    int      value;
    uint8_t  active;
    char     pad0[3];
};

struct TargetEntry
{
    int      index;
    int      pad0;
    int      state;
    int      team;
    float    dist;
    float    fov;
    char     pad1[0x28];
};

struct SolveContext;

// Плоский список записей: пара «начало/конец» без владения памятью.
struct ShotList
{
    char* begin;
    char* end;

    void Release()
    {
        if (begin)
        {
            free(begin);
            begin = nullptr;
            end   = nullptr;
        }
    }
};

// Снимок состояния, с которым запускается сбор точек прицеливания.
// Раскладка повторяет структуру из решателя: 40 байт, поля читаются
// сборщиком точек напрямую по смещениям.
struct AimQuery
{
    uint8_t  flag0;      // 0x00 повторный проход по готовому списку
    char     pad0[3];
    int      tickNow;    // 0x04 тик, к которому относится снимок
    void*    list;       // 0x08 список предсказаний (записи по 172 байта)
    int      index;      // 0x10 выбранная запись снимка
    float    frac;       // 0x14 дробная часть времени снимка
    int      mode;       // 0x18 режим решателя
    uint8_t  flag1C;     // 0x1C разрешить поиск точки через FindAimPoint
    uint8_t  flag1D;     // 0x1D искать в истории тик с максимальным уроном
    uint8_t  flag1E;     // 0x1E не подмешивать точку из истории
    char     pad1;
    int      field20;    // 0x20
    int      field24;    // 0x24
};

// Контекст прохода по стоп-пути.
struct StopParams
{
    int      slot;
    int      hitCount;
    int      budget;
    SolveContext* ctx;
    uint8_t  flag82;
    uint8_t  flag81;
    uint8_t  flag84;
    uint8_t  flag118;
    uint8_t  flag119;
    uint8_t  flag208;
    char     pad0[2];
    int      mode;
    void*  (*callback)();
    char*    extra0;
    char*    extra1;
};

// Рабочая область стоп-пути: переиспользуется между проходами.
struct StopPath
{
    char     storage[0x400];
    int      count;
    uint8_t  abort;
    uint8_t  skip;
    char     pad0[2];
};

// Смещения LocalData, на которые опирается снапшот настроек.
namespace lc
{
    constexpr int pawn      = 0x150;
    constexpr int weapon_id = 0x7B8;
}

// ============================================================================
//  состояние
// ============================================================================
static LocalData*    g_local;
static void*         g_solveCtx;
static CmdResult*    g_result;
static RageContext*  g_rage;
static RageSettings* g_settings;
static CmdRecord*    g_records;

static ShotPos* g_shotsBegin;
static ShotPos* g_shotsEnd;
static ShotPos* g_shotsCap;

static TargetEntry* g_targets;
static TargetEntry* g_targetsEnd;
static TargetEntry* g_targetsCap;
static int          g_best[3] = { -1, -1, -1 };

static bool g_enabled;
static bool g_silent;
static bool g_refineShot;

static uint64_t g_cmdFlags;
static uint64_t g_tickFn;
static int      g_tickFnStride;
static int      g_tickFnCount;
static uint64_t g_schemaSystem;

static char  g_viewAngles[32];
static char  g_aimAngles[32];
static float g_shotState[8];
static char  g_solveQueue[64];
static char  g_shotInfo[32];
static char  g_shotWeapon[16];
static char  g_shotAmmo[8];
static char  g_shotStage[16];
static char  g_shotExtra[16];
static char  g_shotSpread[16];
static int   g_spreadStride;
static char* g_targetList;
static char* g_targetListEnd;
static char  g_aimListA[64];
static char  g_aimListB[64];
static char  g_aimHits[16];
static char  g_aimHitsEnd[16];
static char  g_aimResults[16];
static char  g_aimResultsEnd[16];
static uint64_t g_traceFnA;
static uint64_t g_traceFnB;
static int   g_aimDescriptorCount;
static uint64_t g_aimDescriptorData;
static int   g_aimDescriptorSize;
static int   g_aimDescriptorFlag;
static int   g_entityOffset;
static int   g_aimDescriptor;
static int   g_latencyA;
static char* g_groupTable;
static int   g_off_m_nZoomLevels;       // CCSWeaponBaseVData::m_nZoomLevels
static int   g_off_m_nNumBullets;       // CCSWeaponBaseVData::m_nNumBullets
static bool  g_zoomOffsetsReady;            // таблица per-group значений

// глобалы конфигурации rage
static int      cfg_groups;      // ragebot.groups.        (кол-во групп)
static int      cfg_aim_speed;      // ragebot.limiters.aim_speed
static int      cfg_reaction_time;      // ragebot.limiters.reaction_time
static int      cfg_kill_delay;      // ragebot.limiters.kill_delay
static int      cfg_max_walls;      // ragebot.limiters.max_walls
static int      cfg_backtrack;      // ragebot.limiters.backtrack
static int      cfg_history;      // ragebot.history
static int      cfg_future_ticks;      // ragebot.future_ticks
static int      cfg_remove_spread_mode;      // ragebot.remove_spread_mode
static uint8_t  cfg_automatic_fire;       // ragebot.automatic_fire
static uint8_t  cfg_limiters;       // ragebot.limiters
static float    cfg_field_of_view;      // ragebot.field_of_view
static float    cfg_limiters_aim_fov;   // ragebot.limiters.aim_fov
static uint8_t  cfg_selftrack;       // ragebot.limiters.selftrack
static uint8_t  cfg_unnamed;       // ragebot.limiters.unnamed
static uint8_t  cfg_delay_shot;       // ragebot.delay_shot (бит 0)
static uint8_t  cfg_delay_shot_alt;       // ragebot.delay_shot (бит 1)
static uint8_t  cfg_duck_peek;       // ragebot.duck_peek_assist
static uint8_t  cfg_double_precision;       // ragebot.double_precision
static uint8_t  cfg_double_tap;       // ragebot.double_tap

// группы
int   ResolveGroupForWeapon(int* groups, int weaponId);
float GetCurrentSpread();
float GetSpreadLimit();

// Заполнитель результата команды: пишет байты +3..+7 и числовые поля,
// которые читает снимок настроек. Вызывается до SnapshotSettings.
void FillCommandResult(const void* angles);

// Хук уточнения: если он зарегистрирован и вернул true, значение из него
// подменяет шаг уточнения по умолчанию.
typedef bool (*RefineHookFn)(float* value);
static RefineHookFn g_refineHook;

// Дистанция из того же хука: заполняется отдельным проходом и читается
// заполнителем результата в полях +0x10 и +0x14.
static float g_hookDistance;

static void SetRefineHook(RefineHookFn hook)
{
    g_refineHook = hook;
}

// Дистанция берётся из того же хука, что и шаг уточнения: если хук ничего
// не вернул, остаётся ноль. Вызывается перед заполнителем результата.
static void UpdateHookDistance()
{
    float value = 0.f;

    g_hookDistance = (g_refineHook && g_refineHook(&value)) ? value : 0.f;
}
static float g_latencyB;
static char  g_bestHit[16];
static int   g_bestDamage;
static char  g_aimSlots[3 * 4104];
static int   g_aimSlotCount;

static ShotMarker g_shotMarker0;
static ShotMarker g_shotMarker1;
static bool       g_shotDirty;

static char* g_scanBegin;
static char* g_scanEnd;
static char* g_scanCap;

static char* g_listBegin;
static char* g_listEnd;

static uint64_t g_scanCmd;
static uint64_t g_scanAngles;
static uint64_t g_scanButtons;
static uint64_t g_scanCallback;
static uint64_t g_scanList0, g_scanList1, g_scanList2, g_scanList3;
static uint64_t g_aimPunch;
static int      g_solveFlags;
static int      g_shotSlot;
static int      g_gameState;
static int      g_lastScanTick;
static float    g_lastScanTime;

static bool g_moduleReady;
static bool g_logReady;
static bool g_forceStop;
static bool g_skipStop;

// Состояние сканирования и стоп-пути.
static char       g_scanData[64];
static StopPath   g_stopPath;
static bool       g_stopDisabled;
static bool       g_stopPathAbort;
static uint8_t    g_stopPathSkip;

// Метка этапа для телеметрии выстрела: пишется только когда трекинг включён.
static void MarkShotStage(int stage)
{
    if (g_shotState[0] != 0.f && stage >= 0 && stage < 8)
        g_shotState[stage] = 1.f;
}

#define MARKER(stage) MarkShotStage(stage)

// ============================================================================
//  схема
// ============================================================================
static int  off_m_WeaponType;
static int  off_m_WeaponCategory;
static int  off_m_bHasBurstMode;
static int  off_m_nZoomLevels;
static int  off_m_bIsFullAuto;
static int  off_m_iMaxClip1;
static int  off_m_pReserveAmmo;
static int  off_m_bFireBulletsSeedSynced;
static int  off_m_nTickBase;
static bool g_offsetsReady;

static int  off_m_hPawn;
static int  off_m_hPlayerPawn;
static int  off_m_iConnected;
static int  off_m_pGameSceneNode;
static int  off_m_lifeState;
static int  off_m_nActualMoveType;
static int  off_m_bGunGameImmunity;
static int  off_m_pCollision;
static int  off_m_vecSurroundingMins;
static int  off_m_vecSurroundingMaxs;
static int  off_m_vecAbsOrigin;
static int  off_m_pMovementServices;
static int  off_m_flDuckAmount;
static int  off_m_hController;
static bool g_targetOffsetsReady;

// ============================================================================
//  внешние функции
// ============================================================================
struct SolveContext;

int   GetSchemaOffset(const char* className, const char* fieldName);
bool  Readable(const void* p);
int   ReadInt(const void* p);
uint8_t ReadByte(const void* p);
void* ReadPtr(const void* p);
void* ReadVec(void* out, const void* p);
void* GetPlayerController(int index);

void  FillCmdRecord(void* dst, void* src, int count, void* extra);
const void* GetExtraTicks(const void* pred);
void  CommitTarget(void* a, void* b, int c, int d);
void  UpdateShotTelemetry(void* angles);
void  ScanTargets(void* rage, void* local, void* cmd, void* pawn, void* weapon);
void  ApplyLimiters(void* cmd);
void* BuildSolveRequest(void* out, void* rage, void* angles, void* buttons, void* slot);
void  QueueSolveResult(void* queue, void* result);
void  CopyViewAngles(void* dst, const void* src);
void  BuildPredictRequest(void* out, void* angles, void* a, int b, int c);
void  BuildPosList(void* out, unsigned count);
void  AppendPos(void* list, void* entry);
void  UpdateShotBookkeeping(void* rage);
void  OnScanTick(int mode, void* ctx, void* shot);

void  StartLogThread();
void  ResetWhy();

void  StopPass(SolveContext* ctx);
void  QuickStop(SolveContext* ctx);
bool  StopVariant(SolveContext* ctx);
void  StopPass2(SolveContext* ctx);

bool  RefineShot(SolveContext* ctx);
bool  BuildShotState(SolveContext* ctx);
bool  PickBest(SolveContext* ctx);
void  FallbackSolve(SolveContext* ctx);

void  KnifeBot(int mode, void* cmd, uint64_t* buttons);

void  ReserveTargets(int count);
void  InsertTarget(int slot, float score, int index);
void  GrowShotList(ShotPos** begin, ShotPos* end, const ShotPos* value);

int   ShotSlotCount(int slot);
int   MaxShotSlots();
bool  RayTest(void* a, void* b);
void  DropTarget(void* rage, void* target);
bool  FinalShotCheck(void* rage, void* target, void* record, int flag);

void  RunSolvePass(void* out, void* request, int a, int b);
void  ApplySolveResult(void* request, void* shot, void* buttons);
void  DestroyTemp(void* request);
bool  CanStillShoot(void* obj);
void* GetEntityFromHandle(int handle);
void* TraceToTarget(void* ctx, unsigned flags, int tick, void* aim, void* target);

extern void* g_worldEntity;

// ============================================================================
//  утилиты
// ============================================================================
static inline CmdRecord* GetRecord(int command)
{
    CmdRecord* record = &g_records[command % 150];
    return (record->command == command) ? record : nullptr;
}

static inline bool WeaponHasSeedSync(void* weaponData)
{
    if (!weaponData || !off_m_bFireBulletsSeedSynced)
        return false;

    const char* field = (const char*)weaponData + off_m_bFireBulletsSeedSynced;
    return IsValid(field) && *(const int*)field == 1;
}

static inline int WeaponInt(void* weaponData, int offset)
{
    if (!weaponData || !offset)
        return 0;

    const char* field = (const char*)weaponData + offset;
    return IsValid(field) ? *(const int*)field : 0;
}

static inline void RemoveShots(uint16_t mask)
{
    ShotPos* dst = g_shotsBegin;

    for (ShotPos* it = g_shotsBegin; it != g_shotsEnd; ++it)
    {
        if (it->flags & mask)
            continue;

        *dst++ = *it;
    }

    g_shotsEnd = dst;
}

static inline void AddShot(const ShotPos& shot)
{
    if (g_shotsEnd != g_shotsCap)
        *g_shotsEnd++ = shot;
    else
        GrowShotList(&g_shotsBegin, g_shotsEnd, &shot);
}

static inline bool NoTargets()
{
    return g_targets == g_targetsEnd;
}

// ============================================================================
//  резолв схемы
// ============================================================================
static void ResolveWeaponOffsets()
{
    g_offsetsReady            = true;
    off_m_WeaponType          = GetSchemaOffset("CCSWeaponBaseVData",     "m_WeaponType");
    off_m_WeaponCategory      = GetSchemaOffset("CCSWeaponBaseVData",     "m_WeaponCategory");
    off_m_bHasBurstMode       = GetSchemaOffset("CCSWeaponBaseVData",     "m_bHasBurstMode");
    off_m_nZoomLevels         = GetSchemaOffset("CCSWeaponBaseVData",     "m_nZoomLevels");
    off_m_bIsFullAuto         = GetSchemaOffset("CCSWeaponBaseVData",     "m_bIsFullAuto");
    off_m_iMaxClip1           = GetSchemaOffset("CBasePlayerWeaponVData", "m_iMaxClip1");
    off_m_pReserveAmmo        = GetSchemaOffset("C_BasePlayerWeapon",     "m_pReserveAmmo");
    off_m_bFireBulletsSeedSynced =
                                GetSchemaOffset("CCSPlayerController",    "m_bFireBulletsSeedSynchronized");
    off_m_nTickBase           = GetSchemaOffset("CBasePlayerController",  "m_nTickBase");
}

static void ResolveTargetOffsets()
{
    g_targetOffsetsReady   = true;
    off_m_hPawn            = GetSchemaOffset("CBasePlayerController", "m_hPawn");
    off_m_hPlayerPawn      = GetSchemaOffset("CCSPlayerController",   "m_hPlayerPawn");
    off_m_iConnected       = GetSchemaOffset("CBasePlayerController", "m_iConnected");
    off_m_pGameSceneNode   = GetSchemaOffset("C_BaseEntity",          "m_pGameSceneNode");
    off_m_lifeState        = GetSchemaOffset("C_BaseEntity",          "m_lifeState");
    off_m_nActualMoveType  = GetSchemaOffset("C_BaseEntity",          "m_nActualMoveType");
    off_m_bGunGameImmunity = GetSchemaOffset("C_CSPlayerPawn",        "m_bGunGameImmunity");

    if (!off_m_bGunGameImmunity)
        off_m_bGunGameImmunity = GetSchemaOffset("C_CSPlayerPawnBase", "m_bGunGameImmunity");

    off_m_pCollision         = GetSchemaOffset("C_BaseEntity",       "m_pCollision");
    off_m_vecSurroundingMins = GetSchemaOffset("CCollisionProperty", "m_vecSurroundingMins");
    off_m_vecSurroundingMaxs = GetSchemaOffset("CCollisionProperty", "m_vecSurroundingMaxs");
    off_m_vecAbsOrigin       = GetSchemaOffset("CGameSceneNode",     "m_vecAbsOrigin");
    off_m_pMovementServices  = GetSchemaOffset("C_BasePlayerPawn",   "m_pMovementServices");
    off_m_flDuckAmount       = GetSchemaOffset("CCSPlayer_MovementServices", "m_flDuckAmount");

    if (!off_m_flDuckAmount)
        off_m_flDuckAmount = GetSchemaOffset("CPlayer_MovementServices_Humanoid", "m_flDuckAmount");

    off_m_hController = GetSchemaOffset("C_BasePlayerPawn", "m_hController");
}

// ============================================================================
//  сбор целей
// ============================================================================
static void CollectTargets(void* query)
{
    if (!g_targetOffsetsReady && g_schemaSystem)
        ResolveTargetOffsets();

    g_targetsEnd = g_targets;

    if ((g_targetsCap - g_targets) < 65)
        ReserveTargets(65);

    const float* view = (const float*)query + 5;
    float threshold[3] = { 0.f, 0.f, 0.f };

    for (int i = 1; i <= 64; ++i)
    {
        void* controller = GetPlayerController(i);
        if (!controller)
            continue;

        void* pawn = ReadPtr((const char*)controller + off_m_hPawn);
        if (!pawn || !IsValid((const char*)pawn + off_m_lifeState))
            continue;

        if (ReadInt((const char*)pawn + off_m_lifeState) != 0)
            continue;

        if (off_m_nActualMoveType && ReadInt((const char*)pawn + off_m_nActualMoveType) == 8)
            continue;

        if (off_m_bGunGameImmunity
            && IsValid((const char*)pawn + off_m_bGunGameImmunity)
            && ReadInt((const char*)pawn + off_m_bGunGameImmunity))
            continue;

        void* node = ReadPtr((const char*)pawn + off_m_pGameSceneNode);
        if (!node)
            continue;

        void* originPtr = ReadVec(nullptr, (const char*)node + off_m_vecAbsOrigin);
        if (!IsValid(originPtr))
            continue;

        Vector center = *(const Vector*)originPtr;

        // смещаемся в центр коллизии
        if (off_m_pCollision)
        {
            void* collision = ReadPtr((const char*)pawn + off_m_pCollision);
            if (collision)
            {
                void* mins = ReadVec(nullptr, (const char*)collision + off_m_vecSurroundingMins);
                void* maxs = ReadVec(nullptr, (const char*)collision + off_m_vecSurroundingMaxs);

                if (IsValid(mins) && IsValid(maxs))
                {
                    const Vector& mn = *(const Vector*)mins;
                    const Vector& mx = *(const Vector*)maxs;

                    center = Vector((center.x + mn.x + center.x + mx.x) * 0.5f,
                                    (center.y + mn.y + center.y + mx.y) * 0.5f,
                                    (center.z + mn.z + center.z + mx.z) * 0.5f);
                }
            }
        }

        const Vector toTarget = center - Vector(view[0], view[1], view[2]);
        if (toTarget.LengthSqr() == 0.f)
            continue;

        const Vector dir = toTarget.Normalized();
        const float dot = dir.x * view[1] + dir.y * view[0] + dir.z * view[2];

        int slot;
        if      (dot > threshold[0]) slot = 0;
        else if (dot > threshold[1]) slot = 1;
        else if (dot > threshold[2]) slot = 2;
        else                         continue;

        InsertTarget(slot, dot, i);

        for (int k = 0; k < 3; ++k)
            threshold[k] = (g_best[k] >= 0) ? g_targets[g_best[k]].fov : 0.f;
    }
}

// ============================================================================
//  просчёт выстрела
// ============================================================================
// ============================================================================
//  объявления хелперов
// ============================================================================
bool  CommitShot(SolveContext* ctx);

// выбор записи прицеливания
static int    g_aimOffsetA, g_aimOffsetB, g_aimOffsetC, g_aimOffsetD;
static int    g_aimOffsetE, g_aimOffsetF, g_aimOffsetG, g_aimOffsetH;
static int    g_aimOffsetI, g_aimOffsetJ;
static float  g_weightTable[128];
static char   g_aimBatch[64];

void  ResetAimState();
void  PackAimVectors(void* ctx, void* list);
void  RegisterAimRecord(void* ctx, void* list, uint64_t a, int b);
void  ComputeSlotState(void* request, void* slot, void* inner, int tick, void* extra);
bool  PrepareAimEntry(void* base, void* entry, void* extra);
void  BuildTargetState(void* out, void* data);
void  BuildCandidates(void** out, void** outEnd, void* base, void* entry, int index, void* state);
void  TestAimHit(void* base, void* entry, void* point, void* candidate);
void  StoreAimHit(void* entry, void* candidate, void* point);
void  ReleaseCandidates(void* candidates);
void  ComputeAngle(const Vector& d, float* x, float* y);
void* AdjustAngle(const void* d, void* ctx, float a, float b);
float GetAngleX();
float GetAngleY();
bool  FindAimPoint(void* ctx, bool knife, void** out);
void  GrowScanList();
float ComputeDamage(void* ctx, bool knife);
float GetTimeFrac(float value, float* out);
float GetAngleScale(float angle);
void* GetViewAngles();
extern void* g_viewEntity;
extern void* g_viewTarget;
float GetTickInterval(float value);
uint16_t ReadWord(const void* p);
bool  Readable(const void* p);
void  ReserveWeightTable(void* table, int count);
uint32_t HashFloat(uint32_t v);
void  SelectAimEntry(void* base, void* entry);
void* FinishAimList(void* base);
void* GetThreadPool();
int   GetPoolWorkers(void* pool);
void  RunAimBatch(void* batch, int mode);
void  DispatchAimBatch(void* pool, void* batch, int a, int b, int c);
bool  CanBuildShot(void* ctx, void* record);
void  PrepareAimRecord(void* ctx, void* record);
bool  CheckAimRecord(void* record, int a, uint8_t b);
void* GetTargetData(void* out, void* target);
void  AppendTargetData(void* list, void* data);
void  UpdateShotSpread(void* obj);
void  ComputeShotSpread();
float GetShotSpread(void* obj);
float ReadFloat(const void* p);
void  PushShotOrigin(void* list, void* origin);
void  ClearShotList(void* list);
void  RebuildAimList(void* ctx, void* list, void* record);
void  CollectAimPoints(void* query, void* record, void* a, void* b, void* out);
void  SortAimPoints(void* ctx, void* list, void* record);
void  FinishShotSolve(void* ctx);
void  ApplySolveResult(void* request, void* shot, void* buttons);
void  RunSolvePass(void* out, void* request, int a, int b);
void  DestroyTemp(void* request);
bool  CanStillShoot(void* obj);
void* GetEntityFromHandle(int handle);
void* TraceToTarget(void* ctx, unsigned flags, int tick, void* aim, void* target);
void  SolveStopPath(void* params, void* path, void* entry, int a, int b, float c);
void  FinishStop(void* params, int a, int b);
void  BuildStopEntry(void* out, void* src);
int   ShotCount(const ShotList& list);
void  BuildStopRequest(void* tickEntry, void* out, float* lateral);
void  ComputeStopDir(void* a, void* b, float* outX, float* outY);
bool  HasFlagInList(const char* begin, const char* end, uint32_t flag);
void  InitStopPath(void* path, void* src);
void  ResetStopPath();
int   GetStopEntryIndex(int slot);
int   GetHitCount(int slot);
int   GetStopBudget();
ShotSlot ShotSlotIndex(int tick);
StopParams MakeStopParams(void* ctx);
void  TruncateList(void* begin, void* end);
void* GetAimPointData(void* out, void* ctx, void* entry);
bool  TryRefine(void* a, void* b, void* entry, uint32_t flags, int e);
void  MergeSolveResult(void* dst, void* request);
void* RebuildAimEntry(void* list);
bool  ApplyAimMiss(void* a, int mode, void* list, void* entry, int flag);
void  WriteShotResult(void* a, void* entry, void* hitInfo, void* target,
                      void* extra, bool knife, void* buttons);
void  ApplyFinalAim(void* a, void* owner, void* extra);
void  AfterShot(void* ctx, void* entry);
void  LogShotEvent(void* ctx, void* entry);

// ---------------------------------------------------------------------------
//  Обёртки над общими помощниками
// ---------------------------------------------------------------------------
int   SchemaOffset(const char* className, const char* fieldName);
void* ReadPtrGuarded(const void* p);
void  CopyShotRecord(void* dst, const void* src);
void  AddAimPoint(void* ctx, void* list, void* point, void* record, void* extra,
                  float angleMax, float radius, void* target, const float* offset);
void  FinalizeFallback(void* ctx, void* list, void* target, int flag);
bool  ApplyAimHit(void* base, void* entry, void* point, void* candidate);
StopParams MakeStopParams(void* ctx);

void* PickAimEntry(void* list, void* extra);
void  FinalizeAimPoints(void* ctx, void* list);
void  ValidateAimSlot(void* slot, int tick);

// ---------------------------------------------------------------------------
//  Обёртки над общими помощниками
// ---------------------------------------------------------------------------
void* TraceRay(void* traceFn, void* target, void* out);
void  TraceAim(void* target, void* a, void* b);
void* ResolveEntity(void* entity);
void  AppendEntry(void* list, void* entry);
void  StoreAimResult(void* aimEntry, void* data, void* entry, void* point, int slot);
void  InitAimEntry(void* entry, void* src);
void  SetAimTarget(void* a, void* b, void* c);
void  CopyRecord(void* dst, const void* src);
void  CopyAimRecord(void* dst, const void* src);
void  GrowAimList(void* list, void* end, void* entry);
void  GrowRecordList(void* list, void* end, void* entry);
void  MergeSolve(void* a, void* b);
void  ShotBookkeeping(void* ctx);
float GetShotTime(void* ctx);
float GetTimeFraction(void* pair);
void  ApplyFinalAim2(void* a, void* owner, void* extra);

// ============================================================================
//  уточнение выстрела
//
//  Собирает точки по трём слотам, выбирает лучшую, проверяет время и
//  видимость, затем применяет результат. Возвращает true, если выстрел
//  уже доведён и пересчёт не нужен.
// ============================================================================
bool RefineShot(SolveContext* ctx)
{
    int flags = 1;

    // нож идёт по другой ветке
    uint8_t allowed = *(uint8_t*)((char*)ctx + 115);

    if (*(int*)((char*)ctx + 96) == 2 && *(uint16_t*)((char*)ctx + 4448) == 64)
    {
        allowed = *(uint8_t*)((char*)ctx + 116);
        flags   = 2049;
    }

    if (!allowed
        || (!*(uint8_t*)((char*)ctx + 9) && (flags & *(uint64_t*)((char*)ctx + 4488)) == 0))
    {
        return false;
    }

    // ---------------------------------------------------------- подготовка
    if (*(uint8_t*)((char*)ctx + 196)
        || *(int*)((char*)ctx + 572) != *(int*)((char*)ctx + 2820))
    {
        FillCmdRecord((char*)ctx + 224, (char*)ctx + 2472, 0, nullptr);

        int count = GetHitCount(*(int*)((char*)ctx + 188));

        if (count > 0 && count <= GetStopBudget())
        {
            const int savedTime = *(int*)((char*)ctx + 580);
            const float time = (float)*(int*)((char*)ctx + 512) * 0.015625f;

            *(float*)((char*)ctx + 580) = time;

            *(int*)((char*)ctx + 2104) = *(int*)((char*)ctx + 236);
            *(int*)((char*)ctx + 2108) = *(int*)((char*)ctx + 240);
            *(int*)((char*)ctx + 2112) = *(int*)((char*)ctx + 244);
            *(int*)((char*)ctx + 2116) = *(int*)((char*)ctx + 328);
            *(uint8_t*)((char*)ctx + 2120) = *(uint8_t*)((char*)ctx + 336);
            *(uint8_t*)((char*)ctx + 2121) = *(uint8_t*)((char*)ctx + 338);
            *(uint8_t*)((char*)ctx + 2122) = *(uint8_t*)((char*)ctx + 394);
            *(int*)((char*)ctx + 2124) = *(int*)((char*)ctx + 572);
            *(float*)((char*)ctx + 2128) = time;
            *(int*)((char*)ctx + 2132) = *(int*)((char*)ctx + 584);
            *(int*)((char*)ctx + 2136) = *(int*)((char*)ctx + 948);
            *(int*)((char*)ctx + 2140) = *(int*)((char*)ctx + 1084);
            *(int*)((char*)ctx + 2144) = *(int*)((char*)ctx + 1088);

            // прогоняем spread столько раз, сколько попаданий
            do
            {
                UpdateShotSpread((char*)ctx + 2104);
                --count;
            }
            while (count);

            *(int*)((char*)ctx + 580)  = savedTime;
            *(int*)((char*)ctx + 2128) = savedTime;
        }
    }

    // ------------------------------------------------------- запрос точки
    char request[128];

    *(uint64_t*)((char*)ctx + 8360) = **(uint64_t**)((char*)ctx + 4472);

    BuildSolveRequest(request, ctx, (char*)ctx + 2336,
                      *(void**)((char*)ctx + 8360), (char*)ctx + 2416);

    if (((*(uint32_t*)((char*)ctx + 328) & 1) != 0 || *(uint8_t*)((char*)ctx + 83))
        && *(uint8_t*)((char*)ctx + 81))
    {
        uint8_t limit = *(uint8_t*)((char*)ctx + 84);

        if ((*(uint32_t*)((char*)ctx + 328) & 1) != 0
            && *(uint8_t*)((char*)ctx + 118)
            && *(int*)((char*)ctx + 572) - *(int*)((char*)ctx + 868) >= 17)
        {
            limit = 0;
        }

        RunSolvePass((char*)ctx + 224, request, limit, *(uint8_t*)((char*)ctx + 208) == 0);
    }

    // обрезаем список до 31 записи (шаг 72)
    {
        char* begin = *(char**)&request[0x40];
        char* end   = *(char**)&request[0x50];

        while ((uint64_t)((end - begin) / 72) >= 0x1F)
        {
            char* it = end;

            while (it != begin)
            {
                char* prev = it - 72;
                if (!*(uint64_t*)(prev + 8))
                    break;

                it = prev;
                if (it == begin)
                    goto trimmed;
            }

            memmove(it - 72, it, (size_t)(end - it));
            end = *(char**)&request[0x50];
            begin = *(char**)&request[0x40];

        trimmed:
            end -= 72;
            *(char**)&request[0x50] = end;
        }
    }

    if (*(int*)((char*)ctx + 96) == 2 && *(uint8_t*)((char*)ctx + 208))
        *(int*)((char*)&request[0x3C]) = 0x42300000;      // 44.0f

    ApplySolveResult(request, (char*)ctx + 2336, (char*)ctx + 8360);
    RunSolvePass((char*)ctx + 224, request, 0, 0);
    MergeSolveResult((char*)ctx + 8368, request);

    *(uint8_t*)((char*)ctx + 8496) = 1;

    // ------------------------------------------------------ сбор списка точек
    *(uint16_t*)((char*)ctx + 4576) = 0;
    *(uint64_t*)((char*)ctx + 4744) = 0;
    *(uint64_t*)((char*)ctx + 4496) = 0;
    *(uint16_t*)((char*)ctx + 4776) = 0;

    TruncateList((char*)ctx + 4528, (char*)ctx + 4536);
    TruncateList((char*)ctx + 4552, (char*)ctx + 4560);
    TruncateList((char*)ctx + 4720, (char*)ctx + 4728);

    RebuildAimList(ctx, (char*)ctx + 4496, (char*)ctx + 224);

    {
        AimQuery query;

        query.flag0   = *(uint8_t*)((char*)ctx + 64);
        query.tickNow = *(int*)((char*)ctx + 2820);
        query.list    = &g_scanData;
        query.index   = *(int*)((char*)ctx + 2316);
        query.frac    = *(float*)((char*)ctx + 2320);
        query.mode    = *(int*)((char*)ctx + 96);
        query.flag1C  = *(uint8_t*)((char*)ctx + 8);
        query.flag1D  = *(uint8_t*)((char*)ctx + 86);
        query.flag1E  = *(uint8_t*)((char*)ctx + 208);
        query.field20 = *(int*)((char*)ctx + 4);
        query.field24 = *(int*)((char*)ctx + 56);

        CollectAimPoints(&query, (char*)ctx + 224, nullptr, nullptr, (char*)ctx + 4552);
    }

    // перенос данных точки в запись (шаг 96)
    for (char* it = *(char**)((char*)ctx + 4552); it != *(char**)((char*)ctx + 4560); it += 96)
    {
        char tmp[32];

        void* res = GetAimPointData(tmp, ctx, it);
        *(uint64_t*)(it + 80) = *(uint64_t*)res;
        *(int*)(it + 88)      = *(int*)((char*)res + 8);
    }

    if (*(float*)((char*)ctx + 28) > 0.f)
    {
        for (int i = 0; i < g_aimSlotCount && i < 3; ++i)
        {
            char* slot = (char*)&g_aimSlots + 4104 * i;

            if (!*(void**)(slot + 8))
                continue;

            int limit = *(int*)((char*)ctx + 32) + *(int*)(slot + 4076);
            const int cap = *(int*)(slot + 4080);

            if (cap < limit)
                limit = cap;

            ValidateAimSlot(slot, limit);
        }
    }

    ClearShotList((char*)ctx + 4496);
    *(uint8_t*)((char*)ctx + 4577) = 1;

    *(int*)((char*)ctx + 8328) = *(int*)((char*)ctx + 188);
    *(int*)((char*)ctx + 192)  = *(int*)((char*)ctx + 188);

    FinalizeAimPoints(ctx, (char*)ctx + 4496);

    void* best = PickAimEntry((char*)ctx + 4496, (char*)ctx + 7888);

    // ------------------------------------------------------------ отбор
    if (best)
    {
        void* target = *(void**)((char*)best + 16);

        if (target && *(float*)((char*)target + 108) <= *(float*)((char*)best + 12))
        {
            if (*(float*)((char*)ctx + 76) > *(float*)((char*)best + 68))
                best = nullptr;
        }
        else
        {
            best = nullptr;
        }
    }

    // уточнение через отдельный тест
    if (*(float*)((char*)ctx + 7948) > 0.f)
    {
        char tmp[32];

        void* res = GetAimPointData(tmp, ctx, (char*)ctx + 8232);
        *(uint64_t*)((char*)ctx + 8312) = *(uint64_t*)res;
        *(int*)((char*)ctx + 8320)      = *(int*)((char*)res + 8);

        if (*(float*)((char*)ctx + 7948) > 0.f)
        {
            if (TryRefine((char*)ctx + 4496, (char*)ctx + 7936, best,
                          *(uint32_t*)((char*)ctx + 4696) & 0xFF, 0))
            {
                best = (char*)ctx + 7936;
            }
        }
    }

    // отметка «цель видна в этом тике»
    if (*(float*)((char*)ctx + 48) > 0.f)
    {
        for (void** it = (void**)g_targetList; it != (void**)g_targetListEnd; ++it)
        {
            void* target = *it;
            if (!target)
                continue;

            void* data = *(void**)((char*)target + 32);
            if (!data)
                continue;

            const int tick = *(int*)((char*)ctx + 2820);
            const uint32_t targetTick = *(uint32_t*)((char*)data + 4044);
            const bool     visible    = (targetTick == (uint32_t)tick);

            if (*(uint8_t*)((char*)target + 60) != (uint8_t)visible)
            {
                *(uint8_t*)((char*)target + 60) = visible;

                *(float*)((char*)target + 56) = visible
                                                    ? (float)tick * 0.015625f
                                                    : 3.4028235e38f;
            }
        }
    }

    if (!best)
        return false;

    // ------------------------------------------------- перенос лучшей точки
    if (*(uint8_t*)((char*)ctx + 112) && g_refineShot)
    {
        memcpy((char*)ctx + 7936, best, 88);

        char* targetBlock = *(char**)((char*)best + 16);
        memcpy((char*)ctx + 8024, targetBlock, 208);

        char* extraBlock = *(char**)((char*)best + 24);
        memcpy((char*)ctx + 8232, extraBlock, 96);

        *(uint64_t*)((char*)ctx + 8264) = 0;
        *(uint64_t*)((char*)ctx + 7952) = (uint64_t)((char*)ctx + 8024);
        *(uint64_t*)((char*)ctx + 7960) = (uint64_t)((char*)ctx + 8232);

        {
            void* owner = *(void**)((char*)targetBlock + 8);

            *(int*)((char*)ctx + 8332) = owner ? *(int*)((char*)owner + 16) : 0;
        }

        {
            void* extra = *(void**)((char*)extraBlock + 8);

            *(int*)((char*)ctx + 8336) = extra ? *(int*)extra : 0;
        }
    }

    // ------------------------------------------------- проверка по времени
    if (*(uint8_t*)((char*)ctx + 36))
    {
        void* target = *(void**)((char*)best + 16);

        if (target)
        {
            void* owner = *(void**)((char*)target + 8);

            if (owner)
            {
                const int index = *(int*)((char*)owner + 16);

                if (index >= 0
                    && index < (int)(g_targetsEnd - g_targets)
                    && g_targets[index].state == 2)
                {
                    const float saved   = *(float*)((char*)ctx + 8340);
                    const float now     = (float)*(int*)((char*)ctx + 2820) * 0.015625f;
                    const float window  = *(float*)((char*)ctx + 48);

                    if (now > saved)
                    {
                        const float limit = saved + *(float*)((char*)ctx + 52);

                        if (window <= 0.f)
                        {
                            if (limit > now)
                                return false;
                        }
                        else
                        {
                            if (limit > now)
                                return false;

                            if (g_targets[index].fov >= (now - window))
                                return false;
                        }
                    }
                    else if (window > 0.f)
                    {
                        if (g_targets[index].fov >= (now - window))
                            return false;
                    }
                }
            }
        }
    }

    // --------------------------------------------------------- видимость
    {
        const Vector point = *(const Vector*)best;
        const Vector aim   = *(const Vector*)((char*)ctx + 164);
        const Vector dir   = *(const Vector*)((char*)ctx + 152);

        if (!RayTest((char*)ctx + 4496, best))
        {
            const Vector delta = point - aim;
            const float  proj  = dir.Dot(delta);
            const float  len   = delta.Length();

            if (len * *(float*)((char*)ctx + 40) > proj)
            {
                DropTarget(ctx, best);
                return false;
            }
        }
    }

    if (FinalShotCheck(ctx, best, (char*)ctx + 224, 1))
        return false;

    // ------------------------------------------- смена режима -> пересборка
    {
        const int mode = *(int*)((char*)ctx + 72);

        if (*(uint8_t*)((char*)ctx + 4696) != (uint8_t)mode)
        {
            *(uint8_t*)((char*)ctx + 4696) = (uint8_t)mode;
            best = RebuildAimEntry((char*)ctx + 4496);
        }
    }

    const bool knife = (*(int*)((char*)ctx + 96) == 2)
                    && (*(uint16_t*)((char*)ctx + 4448) == 64);

    if (!best)
        return false;

    // ---------------------------------------------------------- валидация
    void* targetData = *(void**)((char*)best + 16);
    if (!targetData)
        return false;

    void* extraData = *(void**)((char*)best + 24);
    if (!extraData)
        return false;

    int* hitInfo = *(int**)((char*)best + 40);
    if (!hitInfo)
        return false;

    void* owner = *(void**)((char*)targetData + 8);
    if (!owner)
        return false;

    {
        const int index = *(int*)((char*)owner + 16);

        if (index < 0
            || index >= (int)(g_targetsEnd - g_targets)
            || g_targets[index].state != 2)
        {
            return false;
        }

        void* targetSlot = *(void**)((char*)g_targets + 64 * index + 48);

        const int tickNow = *(int*)((char*)extraData + 12)
                          - (int)((float)(int)g_latencyA + g_latencyB);

        const int budget = *hitInfo;

        // ------------------------------------------------- применение
        if (RayTest((char*)ctx + 4496, best))
        {
            if (!ApplyAimHit((char*)ctx + 4752, best, hitInfo, extraData))
                return false;
        }
        else
        {
            if (!ApplyAimMiss((char*)ctx + 208, *(int*)((char*)ctx + 96),
                              (char*)ctx + 4496, best, budget > tickNow))
            {
                return false;
            }
        }

        **(uint64_t**)((char*)ctx + 4472) = *(uint64_t*)((char*)ctx + 8360);

        WriteShotResult((char*)ctx + 208, best, hitInfo, targetData, extraData,
                        knife, *(void**)((char*)ctx + 8360));

        if (knife)
            **(uint64_t**)((char*)ctx + 4472) &= ~1ull;

        *(uint16_t*)((char*)ctx + 121) = 1;

        if (budget <= tickNow && !*(uint8_t*)((char*)ctx + 36))
        {
            if (targetSlot)
                ValidateAimSlot(targetSlot, *(int*)((char*)targetSlot + 4080));

            void* owner2 = *(void**)((char*)targetData + 8);
            ApplyFinalAim((char*)ctx + 208, owner2, extraData);
        }
    }

    AfterShot(ctx, best);
    LogShotEvent(ctx, best);

    return true;
}

// ============================================================================
//  сборка состояния выстрела
//
//  Готовит запись выстрела: слот, точка, скорость, флаги. Возвращает признак
//  «состояние собрано».
// ============================================================================
bool BuildShotState(SolveContext* ctx)
{
    if (g_shotState[0] != 0.f)
    {
        g_shotWeapon[0]  = 1;
        *(int*)&g_shotWeapon[4]  = *(uint16_t*)((char*)ctx + 4448);
        *(int*)&g_shotWeapon[8]  = *(int*)((char*)ctx + 3352);
        *(int*)&g_shotWeapon[12] = *(int*)((char*)ctx + 4408);

        *(uint16_t*)&g_shotAmmo[0] = *(uint16_t*)((char*)ctx + 2641);

        g_shotStage[8] = 1;
    }

    bool built = CanBuildShot(ctx, (char*)ctx + 2472);
    if (!built)
        return built;

    if (g_shotState[0] != 0.f)
        g_shotStage[8] = 2;

    // слот должен попадать в окно
    {
        const ShotSlot slot = ShotSlotIndex(*(int*)((char*)ctx + 2820));

        built = (slot.index == *(int*)((char*)ctx + 3332))
                    ? slot.frac >= *(float*)((char*)ctx + 3336)
                    : slot.index >= *(int*)((char*)ctx + 3332);

        if (!built)
            return built;
    }

    // ---------------------------------------------------------- solve-проход
    if (*(uint8_t*)((char*)ctx + 196)
        || *(int*)((char*)ctx + 14600) != *(int*)((char*)ctx + 2820))
    {
        char entry[64];
        BuildStopEntry(entry, (char*)ctx + 8504);

        FillCmdRecord((char*)ctx + 12616, (char*)ctx + 2472, 17, nullptr);

        StopParams params = MakeStopParams(ctx);

        SolveStopPath(&params, (char*)ctx + 12616, entry, 8, 8, -1.f);

        // скорость по горизонтали
        {
            const float vx = *(float*)((char*)ctx + 12628);
            const float vy = *(float*)((char*)ctx + 12632);
            const float sq = vx * vx + vy * vy;

            if (g_shotState[0] != 0.f)
            {
                g_shotStage[9] = 1;
                *(float*)&g_shotStage[12] = (sq < 0.f) ? FastSqrt(sq) : sqrtf(sq);
                *(int*)&g_shotExtra[0] = *(int*)((char*)ctx + 12904) - *(int*)((char*)ctx + 2760);
            }
        }

        *(int*)((char*)ctx + 14600) = *(int*)((char*)ctx + 2820);

        if (*(uint8_t*)((char*)ctx + 4488) & 2)
            *(int*)((char*)ctx + 12720) &= ~1;

        // ------------------------------------------------- сброс состояния
        *(uint64_t*)((char*)ctx + 12632) = 0;
        *(int*)((char*)ctx + 12628)      = 0;
        *(int*)((char*)ctx + 14544)      = 0;
        *(int*)((char*)ctx + 14496)      = 0;
        *(uint64_t*)((char*)ctx + 14500) = 0;

        *(int*)((char*)ctx + 14508)      = *(int*)((char*)ctx + 12720);
        *(uint8_t*)((char*)ctx + 14512)  = *(uint8_t*)((char*)ctx + 12728);
        *(uint8_t*)((char*)ctx + 14513)  = *(uint8_t*)((char*)ctx + 12730);
        *(uint8_t*)((char*)ctx + 14514)  = *(uint8_t*)((char*)ctx + 12786);
        *(int*)((char*)ctx + 14516)      = *(int*)((char*)ctx + 12964);
        *(int*)((char*)ctx + 14520)      = *(int*)((char*)ctx + 12972);
        *(int*)((char*)ctx + 14524)      = *(int*)((char*)ctx + 12976);
        *(int*)((char*)ctx + 14528)      = *(int*)((char*)ctx + 13340);
        *(int*)((char*)ctx + 14532)      = *(int*)((char*)ctx + 13476);
        *(int*)((char*)ctx + 14536)      = *(int*)((char*)ctx + 13480);

        UpdateShotSpread((char*)ctx + 14496);
    }

    // ------------------------------------------------------ очистка списков
    *(uint16_t*)((char*)ctx + 7120) = 0;
    *(uint64_t*)((char*)ctx + 7288) = 0;
    *(uint64_t*)((char*)ctx + 7040) = 0;
    *(uint16_t*)((char*)ctx + 7320) = 0;

    TruncateList((char*)ctx + 7072, (char*)ctx + 7080);
    TruncateList((char*)ctx + 7096, (char*)ctx + 7104);
    TruncateList((char*)ctx + 7264, (char*)ctx + 7272);

    // ------------------------------------------------- смещение точки
    float spread = 0.f;

    {
        float origin[3];

        origin[0] = *(float*)((char*)ctx + 12652) + *(float*)((char*)ctx + 12616);
        origin[1] = *(float*)((char*)ctx + 12656) + *(float*)((char*)ctx + 12620);
        origin[2] = *(float*)((char*)ctx + 12660) + *(float*)((char*)ctx + 12624);

        const int tick = *(int*)((char*)ctx + 12964);

        if (*(int*)((char*)ctx + 96) != 2 && *(uint16_t*)((char*)ctx + 14592) != 31)
        {
            *(int*)((char*)ctx + 14496) = *(int*)((char*)ctx + 12628);
            *(int*)((char*)ctx + 14500) = *(int*)((char*)ctx + 12632);
            *(int*)((char*)ctx + 14504) = *(int*)((char*)ctx + 12636);
            *(int*)((char*)ctx + 14508) = *(int*)((char*)ctx + 12720);
            *(uint8_t*)((char*)ctx + 14512) = *(uint8_t*)((char*)ctx + 12728);
            *(uint8_t*)((char*)ctx + 14513) = *(uint8_t*)((char*)ctx + 12730);
            *(uint8_t*)((char*)ctx + 14514) = *(uint8_t*)((char*)ctx + 12786);
            *(int*)((char*)ctx + 14516) = tick;
            *(int*)((char*)ctx + 14520) = *(int*)((char*)ctx + 12972);
            *(int*)((char*)ctx + 14524) = *(int*)((char*)ctx + 12976);
            *(int*)((char*)ctx + 14528) = *(int*)((char*)ctx + 13340);
            *(int*)((char*)ctx + 14532) = *(int*)((char*)ctx + 13476);
            *(int*)((char*)ctx + 14536) = *(int*)((char*)ctx + 13480);

            ComputeShotSpread();

            int index = *(int*)((char*)ctx + 14552);
            if (index > 1)
                index = 0;

            void* table = *(void**)((char*)ctx + 14584);
            float extra = 0.f;

            if (table && g_spreadStride)
                extra = ReadFloat((char*)table + g_spreadStride + 4 * index);

            spread = GetShotSpread((char*)ctx + 14496) + extra;
        }

        if (g_shotState[0] != 0.f)
        {
            g_shotSpread[0] = 1;
            *(int*)&g_shotSpread[4] = *(int*)((char*)ctx + 13496);
            *(int*)&g_shotSpread[8] = *(int*)((char*)ctx + 14552);
            *(float*)&g_shotSpread[12] = spread;
        }

        PushShotOrigin((char*)ctx + 7096, origin);
    }

    ClearShotList((char*)ctx + 7040);
    RebuildAimList(ctx, (char*)ctx + 7040, (char*)ctx + 12616);

    // ------------------------------------------------- три слота
    for (int i = 0; i < g_aimSlotCount && i < 3; ++i)
    {
        char* slot = (char*)&g_aimSlots + 4104 * i;

        if (!*(void**)(slot + 8))
            continue;

        int delta = 0;
        if (*(int*)((char*)ctx + 132) - *(int*)((char*)ctx + 32) > 0)
            delta = *(int*)((char*)ctx + 132) - *(int*)((char*)ctx + 32);

        ValidateAimSlot(slot, *(int*)((char*)ctx + 12964) - delta);
    }

    const int savedFlag = *(int*)((char*)ctx + 4);
    *(int*)((char*)ctx + 4) = 0;

    FinalizeAimPoints(ctx, (char*)ctx + 7040);

    void* best = PickAimEntry((char*)ctx + 7040, (char*)ctx + 7888);

    bool accepted = false;

    if (best)
    {
        void* bestTarget = *(void**)((char*)best + 16);
        if (bestTarget && *(float*)((char*)bestTarget + 108) <= *(float*)((char*)best + 12))
            accepted = true;
    }

    *(int*)((char*)ctx + 4) = savedFlag;

    if (g_shotState[0] != 0.f)
        g_shotStage[8] = 3;

    if (accepted)
    {
        if (g_shotState[0] != 0.f)
        {
            g_shotExtra[4]  = 1;
            *(int*)&g_shotExtra[8]  = *(int*)((char*)best + 68);
            *(int*)&g_shotExtra[12] = *(int*)((char*)ctx + 76);
            g_shotStage[8] = 4;
        }

        if (*(float*)((char*)ctx + 76) <= *(float*)((char*)best + 68))
        {
            if (g_shotState[0] != 0.f)
                g_shotStage[8] = 5;

            built = CommitShot(ctx);
        }
    }

    return built;
}

// ============================================================================
//  применение выстрела
//
//  Считает итоговый угол, пишет запись в общий список точек и выставляет
//  кнопки. Ниже — основная ветка (до ножевой).
// ============================================================================
bool ApplyShot(char* ctx, void* pointData, void* best, void* hitInfo,
               bool knife, void* slot)
{
    (void)best;

    float angleX = 0.f;
    float angleY = 0.f;
    uint64_t angleRaw = 0;
    uint32_t tag = 0;

    // ---------------------------------------------------- угол точки
    if (*(uint8_t*)((char*)hitInfo + 12))
    {
        angleRaw = *(uint64_t*)hitInfo;
        tag      = *(uint32_t*)((char*)hitInfo + 8);
        angleX   = *(float*)&angleRaw;
        angleY   = *(float*)((char*)&angleRaw + 4);
    }
    else
    {
        const Vector delta = *(const Vector*)((char*)pointData + 8)
                           - *(const Vector*)((char*)ctx + 0);

        float out[2];
        ComputeAngle(delta, &out[0], &out[1]);

        const void* res = AdjustAngle(&delta, (char*)ctx + 16, 0.f, 0.f);
        const uint32_t extra = *(const uint32_t*)((const char*)res + 8);

        if (*(uint8_t*)ctx || *(uint8_t*)((char*)ctx + 1))
        {
            out[0] -= *(const float*)res;
            out[1] -= *(const float*)((const char*)res + 4);
        }
        else
        {
            out[0] = GetAngleX();
            out[1] = GetAngleY();
        }

        angleRaw = *(uint64_t*)out;
        angleX   = out[0];
        angleY   = out[1];

        tag = *(uint8_t*)ctx ? 0u : (extra ^ 0x80000000u);
    }

    if (*(uint8_t*)((char*)pointData + 12))
    {
        void* found = nullptr;

        if (FindAimPoint((char*)ctx + 16, knife, &found))
        {
            angleRaw = (uint64_t)found;
            angleX   = *(float*)&angleRaw;
            angleY   = *(float*)((char*)&angleRaw + 4);
        }
    }

    // ------------------------------------------ запись в общий список точек
    {
        char* first = g_scanBegin;
        char* last  = g_scanEnd;

        if (first == last)
        {
            GrowScanList();
            first = g_scanBegin;
            last  = g_scanEnd;
        }

        int index = *(int*)(ctx + 2108);

        if (index == -1 || index >= (int)(last - first) / 172)
        {
            index = (int)((last - first) / 172) - 1;
            *(int*)(ctx + 2108) = index;
        }

        float frac = g_latencyB + *(float*)((char*)hitInfo + 4);
        int   tick = g_latencyA + *(int*)hitInfo;

        float fracOut = frac;
        if (frac >= 1.f)
            fracOut = frac - 1.f;

        int tickOut = tick;
        if (frac < 1.f)
            tickOut = tick + 1;

        char* entry = first + 172 * index;

        *(int*)(entry + 24)    = (int)angleRaw;
        *(int*)(entry + 12)    = tickOut;
        *(int*)(entry + 8)     = (int)tag;
        *(float*)(entry + 28)  = angleY;
        *(float*)(entry + 16)  = fracOut;
        *(uint64_t*)entry      = angleRaw;

        *(uint8_t*)(entry + 32)  = 1;
        *(uint8_t*)(entry + 20)  = 1;
        *(uint8_t*)(entry + 120) = 0;
        *(uint8_t*)(entry + 96)  = 0;
        *(uint8_t*)(entry + 104) = 0;
        *(uint8_t*)(entry + 136) = 0;
        *(uint8_t*)(entry + 152) = 0;
        *(uint8_t*)(entry + 168) = 0;
        *(uint8_t*)(entry + 40)  = 0;
        *(uint8_t*)(entry + 56)  = 0;
        *(uint8_t*)(entry + 72)  = 0;
        *(uint8_t*)(entry + 88)  = 0;

        if (*(uint8_t*)((char*)hitInfo + 8))
        {
            memcpy(entry + 44, (char*)hitInfo + 12, 16);
            *(uint8_t*)(entry + 56) = 1;

            memcpy(entry + 60, (char*)hitInfo + 28, 16);
            *(uint8_t*)(entry + 72) = 1;

            *(int*)(entry + 36)     = *(int*)((char*)hitInfo + 44);
            *(uint8_t*)(entry + 40) = 1;
        }

        // -------------------------------------------------- запись углов
        if (*(uint8_t*)((char*)ctx + 1))
        {
            *(uint8_t*)(ctx + 2124) = 1;

            if (!*(uint8_t*)((char*)pointData + 12))
            {
                const float dmg = ComputeDamage((char*)ctx + 16, knife);

                const ShotSlot slotInfo = ShotSlotIndex(*(int*)(ctx + 364));
                const float base      = slotInfo.frac;

                float frac2 = 0.f;
                const float raw = GetTimeFrac(g_local ? g_local->tickTime : 0.f, &frac2);

                float f = base + raw;
                float fOut = f;
                int   iOut = slotInfo.index + (int)frac2;

                if (f >= 1.f)
                    fOut = f - 1.f;
                else
                    iOut = slotInfo.index + (int)frac2;

                *(float*)(ctx + 2120) = fOut;
                *(int*)(ctx + 2116)   = iOut;

                *(float*)&g_viewAngles[0] = angleX - GetAngleScale(angleX / 360.f) * 360.f;
                *(float*)&g_viewAngles[4] = angleY - GetAngleScale(angleY / 360.f) * 360.f;
                *(float*)&g_viewAngles[8] = *(float*)&tag - GetAngleScale(*(float*)&tag / 360.f) * 360.f;

                (void)dmg;
            }
        }
        else
        {
            g_shotSlot = tickOut;
        }
    }

    // ---------------------------------------------------- подрезка углов
    if (!*(uint8_t*)((char*)ctx + 9))
    {
        if (*(uint8_t*)ctx == 0 && !*(uint8_t*)((char*)ctx + 1))
        {
            const Vector delta = *(const Vector*)((char*)pointData + 8)
                               - *(const Vector*)((char*)ctx + 0);

            float out[2];
            ComputeAngle(delta, &out[0], &out[1]);

            if (out[0] < -89.f)  out[0] = -89.f;
            if (out[0] >  89.f)  out[0] =  89.f;
            if (out[1] < -180.f) out[1] = -180.f;
            if (out[1] >  180.f) out[1] =  180.f;

            angleRaw = *(uint64_t*)out;
        }

        if (g_viewEntity)
        {
            if (g_viewTarget)
            {
                void* view = GetViewAngles();

                if (IsValid(view))
                {
                    *(uint64_t*)view = angleRaw;
                    *(int*)((char*)view + 8) = 0;
                }
            }
        }
    }

    // ---------------------------------------------------------- кнопки
    *(uint8_t*)((char*)&g_shotMarker0 + 4) = 0;
    *(uint8_t*)((char*)&g_shotMarker1 + 4) = 0;

    if (knife)
    {
        g_cmdFlags = (g_cmdFlags & 0xFFFFFFFFFFFFF7FEull) | 0x800;

        if (!*(uint8_t*)((char*)pointData + 12))
        {
            *(int*)&g_shotMarker1 = *(int*)(ctx + 2108);
            *(uint16_t*)((char*)&g_shotMarker1 + 5) = *(uint16_t*)((char*)&hitInfo + 5);
            *(uint8_t*)((char*)&g_shotMarker1 + 7)  = *(uint8_t*)((char*)hitInfo + 7);
            *(uint8_t*)((char*)&g_shotMarker1 + 4)  = 1;
        }
    }
    else
    {
        g_cmdFlags |= 1;

        if (!*(uint8_t*)((char*)pointData + 12))
        {
            *(int*)&g_shotMarker0 = *(int*)(ctx + 2108);
            *(uint16_t*)((char*)&g_shotMarker0 + 5) = *(uint16_t*)((char*)&hitInfo + 5);
            *(uint8_t*)((char*)&g_shotMarker0 + 7)  = *(uint8_t*)((char*)hitInfo + 7);
            *(uint8_t*)((char*)&g_shotMarker0 + 4)  = 1;
        }
    }

    // ------------------------------------------------- чистка кольца выстрелов
    if (*(uint16_t*)(ctx + 1992) == 64)
    {
        // нож: убираем записи с флагом 0x801
        ShotPos* dst = g_shotsBegin;

        for (ShotPos* it = g_shotsBegin; it != g_shotsEnd; ++it)
        {
            if (it->flags & 0x801)
                continue;

            *dst++ = *it;
        }

        g_shotsEnd = dst;
    }
    else if (*(uint8_t*)((char*)pointData + 32))
    {
        // обычный выстрел: убираем записи с флагом 1 (или 2048 для ножа)
        if ((uint64_t)((g_shotsEnd - g_shotsBegin) / 72) + 2 <= 0x20)
        {
            const uint32_t mask = knife ? 2048u : 1u;

            ShotPos* dst = g_shotsBegin;

            for (ShotPos* it = g_shotsBegin; it != g_shotsEnd; ++it)
            {
                if (*(uint32_t*)((char*)it + 8) & mask)
                    continue;

                *dst++ = *it;
            }

            g_shotsEnd = dst;

            // и сразу добавляем свою запись
            ShotPos entry{};
            *(int*)((char*)&entry + 0)  = *(int*)((char*)pointData + 28);
            *(uint64_t*)((char*)&entry + 8) = mask;
            *(uint8_t*)((char*)&entry + 16) = 1;

            AddShot(entry);
        }
    }

    return true;
}

// ============================================================================
//  финализация списка точек
//
//  Переносит из решающего контекста в запись: тик, позиции, углы, флаги.
//  Векторная часть (позиции и углы) в бинаре собирается через SSE-shuffle,
//  здесь она вынесена в отдельный вызов.
// ============================================================================
void FinalizeAimPoints(void* ctx, void* list)
{
    char* c = (char*)ctx;
    char* l = (char*)list;

    *(int*)(l + 24)        = *(int*)(c + 2820);      // тик
    *(uint64_t*)l          = *(uint64_t*)(c + 2800);
    *(uint64_t*)(l + 16)   = *(uint64_t*)(c + 4432);

    // углы
    *(uint64_t*)(l + 84)  = *(uint64_t*)(c + 164);
    *(int*)(l + 92)       = *(int*)(c + 172);
    *(uint64_t*)(l + 96)  = *(uint64_t*)(c + 152);
    *(int*)(l + 104)      = *(int*)(c + 160);

    // состояние выстрела
    {
        const int* shot = *(const int**)(c + 4464);

        *(int*)(l + 120) = shot[0];
        *(int*)(l + 124) = shot[1];
        *(int*)(l + 128) = shot[2];
    }

    // векторная часть: позиции, направление, флаги
    PackAimVectors(c, l);

    RegisterAimRecord(c, l, *(uint64_t*)(c + 2800), 1);
}

// ============================================================================
//  проверка слота
//
//  Слот описывает одно записанное положение цели. Если слот ещё жив,
//  пересчитываем его через solve-проход.
// ============================================================================
void ValidateAimSlot(void* slot, int tick)
{
    char* s = (char*)slot;

    void* data = *(void**)(s + 8);
    if (!data)
        return;

    void* inner = *(void**)(s + 32);
    if (!inner)
        return;

    // параметры слота
    const int   slotTick  = *(int*)(s + 16);
    const int   slotExtra = *(int*)(s + 20);
    const uint8_t flag   = *(uint8_t*)(s + 25);

    const int limitA = *(int*)(s + 4076);
    const int limitB = *(int*)(s + 4080);
    const int limitC = *(int*)(s + 4084);
    const int limitD = *(int*)(s + 4088);

    const uint8_t flagA = *(uint8_t*)(s + 4097);
    const uint8_t flagB = *(uint8_t*)(s + 4098);

    // собираем запрос
    char request[128] = {};

    *(uint64_t*)&request[8]  = *(uint64_t*)(s + 16);
    *(int*)&request[12]      = slotExtra;
    *(uint64_t*)&request[16] = slotTick;

    ComputeSlotState(request, slot, inner, tick, s + 2016);

    DestroyTemp(request);
}

// ============================================================================
//  обработка одной записи
//
//  Для каждой точки слота собирается набор кандидатов, и по каждому
//  кандидату прогоняется проверка попадания.
// ============================================================================
void ProcessAimEntry(void* base, void* entry, void* extra)
{
    char* b = (char*)base;
    char* e = (char*)entry;

    if (!PrepareAimEntry(b, e, extra))
        return;

    void* data = *(void**)(e + 8);
    if (!data)
        return;

    // состояние цели для этого прохода
    void* targetState[12] = {};
    BuildTargetState(targetState, data);

    const int total = (int)((*(uint64_t*)(b + 64) - *(uint64_t*)(b + 56)) / 96);

    for (int i = 0; i < total; ++i)
    {
        float* point = (float*)(*(uint64_t*)(b + 56) + 96 * i);

        // кандидаты по этой точке
        void* candidates    = nullptr;
        void* candidatesEnd = nullptr;

        BuildCandidates(&candidates, &candidatesEnd, b, e, i, targetState);

        for (void** it = (void**)candidates; it != (void**)candidatesEnd; ++it)
        {
            void* candidate = *it;

            TestAimHit(b, e, point, candidate);
            StoreAimHit(e, candidate, point);

            // помечаем бит слота, если он разрешён
            if (*(uint8_t*)(b + 81))
                *(int*)((char*)candidate + 16) |= 1 << (i & 0x1F);
        }

        ReleaseCandidates(candidates);
    }
}

// ============================================================================
//  проверка возможности выстрела
//
//  true, если по текущему оружию можно целиться: есть зум, это не одиночный
//  выстрел и цель не находится в состоянии, которое запрещает стрелять.
// ============================================================================
bool CanBuildShot(void* ctx, void* record)
{
    if (!g_zoomOffsetsReady && g_schemaSystem)
    {
        g_zoomOffsetsReady  = true;
        g_off_m_nZoomLevels = SchemaOffset("CCSWeaponBaseVData", "m_nZoomLevels");
        g_off_m_nNumBullets = SchemaOffset("CCSWeaponBaseVData", "m_nNumBullets");
    }

    char* c = (char*)ctx;
    char* r = (char*)record;

    if (!*(uint8_t*)(c + 82))
        return false;

    void* weaponData = *(void**)(r + 1968);
    if (!weaponData)
        return false;

    if (!g_off_m_nZoomLevels)
        return false;

    const int* zoom = (const int*)((char*)weaponData + g_off_m_nZoomLevels);
    if (!IsValid(zoom))
        return false;

    if (*zoom <= 0)
        return false;

    // одиночный выстрел (число патронов == 1) целиться не даёт
    if (*(int*)(r + 1936) == 1)
        return false;

    // цель в запрещённом состоянии
    if (*(uint8_t*)(r + 170) != 0)
        return false;

    return true;
}

// ============================================================================
//  пересборка списка точек
// ============================================================================
void RebuildAimList(void* ctx, void* list, void* record)
{
    char* c = (char*)ctx;
    char* l = (char*)list;

    PrepareAimRecord(c, record);

    *(uint64_t*)(l + 108) = *(uint64_t*)record;
    *(int*)(l + 116)      = *(int*)((char*)record + 8);

    const bool flag = (!*(uint8_t*)(c + 208) || *(int*)(c + 96) != 2);
    *(uint8_t*)(l + 132) = flag;

    const bool ok = CheckAimRecord(record, 0, *(uint8_t*)(c + 8));
    *(uint8_t*)(l + 80) = ok;
}

// ============================================================================
//  очистка списка выстрелов
//
//  Из списка целей берутся те, у которых выставлен флаг +40, и их данные
//  переносятся в список выстрелов (шаг 208 байт).
// ============================================================================
void ClearShotList(void* list)
{
    char* dst = (char*)list + 32;

    char* begin = g_targetList;
    char* end   = g_targetListEnd;

    if (begin == end)
        return;

    for (char* it = begin; it != end; ++it)
    {
        void* target = *(void**)it;

        if (!target)
            continue;

        if (!*(uint8_t*)((char*)target + 40))
            continue;

        char buf[208] = {};

        void* src = GetTargetData(buf, target);
        memcpy(buf, src, 208);

        AppendTargetData(dst, buf);
    }
}

// ============================================================================
//  снимок настроек
//
//  Собирает из глобалов и настроек группы структуру настроек текущей команды.
//
//  Здесь связываются все пять фич:
//    force_shot    -> +0x50 (байт) и +0x18 (хитчанс / 100)
//    hitchance     -> +0x14 (хитчанс / 100), эффективный +0x4C
//    min_damage    -> +0x10 (целое, без /100)
//    backtrack     -> +0x38 (тики)
//    remove_spread -> +0x60 (режим, только для seed-synchronized оружия)
// ============================================================================
void SnapshotSettings()
{
    char* settings = (char*)g_settings;
    char* local    = (char*)g_local;

    // ---------------------------------------------------- выбор группы
    const int groupIndex = ResolveGroupForWeapon(&cfg_groups,
                                                 *(uint16_t*)(local + lc::weapon_id));

    *(int*)settings = groupIndex;

    // количество групп ограничено диапазоном 1..64
    int groups = cfg_groups;

    if (groups < 1)
        groups = 1;
    else if (groups > 64)
        groups = 64;

    // слот группы: значение из настроек берётся, только если оно внутри
    // диапазона групп, иначе берётся последняя группа; отрицательное
    // значение даёт нулевую группу
    int slot;

    if (groupIndex < 0)
        slot = 0;
    else if (groupIndex > groups - 1)
        slot = groups - 1;
    else
        slot = groupIndex;

    const int base = 9 * slot;

    // байты группы: 4 * base + смещение
    auto grpByte = [&](int offset) -> int
    {
        return g_groupTable[4 * base + offset];
    };

    // часть полей группы хранится как 32-битные числа
    auto grpInt = [&](int offset) -> int
    {
        return *(const int*)(g_groupTable + 4 * base + offset);
    };

    // -------------------------------------------------- базовые значения
    *(int*)(settings + 0x04) = cfg_history;          // history
    *(uint8_t*)(settings + 0x08) = cfg_double_tap;       // double_tap
    *(uint8_t*)(settings + 0x09) = cfg_automatic_fire;       // automatic_fire

    const float spread = GetCurrentSpread();

    *(float*)(settings + 0x0C) = spread;

    // --------------------------------------------------- хитчанс и урон
    // эти четыре поля группы читаются как 32-битные числа
    // min_damage - целое, деления на 100 нет
    *(float*)(settings + 0x10) = (float)grpInt(271052);

    // хитчанс и хитчанс force_shot - проценты
    const float hitchance          = (float)grpInt(271040) / 100.f;
    const float forceShotHitchance = (float)grpInt(271048) / 100.f;
    const float extrapolation      = (float)grpInt(271036) / 100.f;

    *(float*)(settings + 0x14) = hitchance;
    *(float*)(settings + 0x18) = forceShotHitchance;
    *(float*)(settings + 0x1C) = extrapolation;

    // ----------------------------------------------------------- лимитеры
    *(uint8_t*)(settings + 0x24) = cfg_limiters;

    // spread - замер до лимитеров, limited - замер, сделанный при включённых
    // лимитерах; из него же получается признак «разброс вырос»
    float limited = spread;

    if (!cfg_limiters)
    {
        // лимитеры выключены - ставим дефолты
        *(float*)(settings + 0x28) = spread;

        // обнуляем пару force_shot_hitchance / extrapolation
        *(uint64_t*)(settings + 0x18) = 0;

        *(int*)(settings + 0x3C) = 4;           // max_walls
        *(int*)(settings + 0x38) = 100;         // backtrack
        *(uint8_t*)(settings + 0x40) = 1;       // selftrack
        *(float*)(settings + 0x2C) = 500.f;     // aim_speed
        *(int*)(settings + 0x44) = 0;           // unnamed
    }
    else
    {
        limited = GetSpreadLimit();

        *(float*)(settings + 0x28) = limited;
        *(float*)(settings + 0x2C) = (float)cfg_aim_speed;              // aim_speed
        *(float*)(settings + 0x30) = (float)cfg_reaction_time / 1000.f;     // reaction_time
        *(float*)(settings + 0x34) = (float)cfg_kill_delay / 1000.f;     // kill_delay
        *(int*)(settings + 0x38)   = cfg_backtrack;                    // backtrack
        *(int*)(settings + 0x3C)   = cfg_max_walls;                    // max_walls
        *(uint8_t*)(settings + 0x40) = cfg_selftrack;                    // selftrack
        *(int*)(settings + 0x44)   = cfg_unnamed ? 0x80 : 0;           // unnamed
    }

    // разброс вырос - отметка.
    // лимитеры выключены - замеров два нет, признак всегда нулевой;
    // при включённом unnamed признак всегда 2, иначе сравниваем замеры.
    {
        int grew = 0;

        if (cfg_limiters)
            grew = cfg_unnamed ? 2 : (limited > spread ? 2 : 0);

        *(int*)(settings + 0x48) = grew;
    }

    // ------------------------------------------- шаг уточнения (refine)
    {
        int step = 100;

        if (extrapolation > 0.f)
        {
            if (extrapolation < 1.f)
            {
                const float t = (extrapolation - 0.2f) / 0.60000002f;

                step = (t >= 0.f) ? (int)(fminf(t, 1.f) * 12.f + 3.f) : 3;
            }
            else
            {
                step = 0;
            }
        }

        *(int*)(settings + 0x20) = step;
    }

    // --------------------------------------------------- force_shot и стопы
    *(uint8_t*)(settings + 0x50) = (uint8_t)grpByte(271044);   // force_shot
    *(uint8_t*)(settings + 0x51) = (uint8_t)grpByte(271056);   // auto_stop
    *(uint8_t*)(settings + 0x52) = (uint8_t)grpByte(271060);   // auto_scope
    *(uint8_t*)(settings + 0x53) = (uint8_t)grpByte(271057);   // stop_in_air
    *(uint8_t*)(settings + 0x54) = (uint8_t)(grpByte(271058) && g_result->gateFlag);
    *(uint8_t*)(settings + 0x55) = (uint8_t)grpByte(271059);   // stop_between_shots
    *(uint8_t*)(settings + 0x56) = cfg_duck_peek;            // duck_peek

    // эффективный хитчанс: хитчанс force_shot берётся только когда и сам
    // force_shot включён, и цель что-то блокирует; иначе - обычный хитчанс
    {
        const float effective = (grpByte(271044) && g_result->field08)
                              ? forceShotHitchance
                              : hitchance;

        *(float*)(settings + 0x4C) = effective;
    }

    *(int*)(settings + 0x58) = cfg_future_ticks;               // future_ticks
    *(uint8_t*)(settings + 0x5C) = cfg_delay_shot;            // delay_shot
    *(uint8_t*)(settings + 0x5D) = cfg_delay_shot_alt;
    *(uint8_t*)(settings + 0x68) = cfg_double_precision;            // double_precision

    // ------------------------------------------------------------ nospread
    // Режим снятия разброса работает только на seed-synchronized оружии:
    // там движок сам считает спред, снимать нечего.
    *(int*)(settings + 0x60) = 0;

    {
        void* controller = *(void**)(local + lc::pawn);

        if (controller
            && off_m_bFireBulletsSeedSynced
            && ReadByte((char*)controller + off_m_bFireBulletsSeedSynced) == 1)
        {
            *(int*)(settings + 0x60) = cfg_remove_spread_mode;
        }
    }

    if (*(int*)(settings + 0x60) == 2)
    {
        // серединный режим: снимаем задержку выстрела и часть флагов
        *(uint8_t*)(settings + 0x5D) = 0;
        *(uint16_t*)(settings + 0x50) = 0;
        *(uint8_t*)(settings + 0x52) = 0;
        *(uint64_t*)(settings + 0x14) = 0;
        *(float*)(settings + 0x4C) = 0.f;
    }

    // -------------------------------------------- развёртка маски хитбоксов
    {
        uint32_t mask = 0;

        if (grpByte(271028)) mask |= 0x00001;

        if (grpByte(271029)) mask |= 0x00002;

        if (grpByte(271030))
        {
            mask |= 0x00004;
            mask |= 0x00008;
        }

        if (grpByte(271029))
        {
            mask |= 0x00010;
            mask |= 0x00020;
            mask |= 0x00040;
        }

        if (grpByte(271032))
        {
            mask |= 0x00080;
            mask |= 0x00100;
        }

        if (grpByte(271033))
        {
            mask |= 0x00200;
            mask |= 0x00400;
            mask |= 0x00800;
            mask |= 0x01000;
        }

        if (grpByte(271031))
        {
            mask |= 0x02000;
            mask |= 0x04000;
            mask |= 0x08000;
            mask |= 0x10000;
            mask |= 0x20000;
            mask |= 0x40000;
        }

        *(int*)(settings + 0x64) = (int)mask;
    }

    // если цель далеко - поднимаем минимальный шаг уточнения
    {
        const int distance = g_result->field14;

        if (distance >= 14)
        {
            int step = *(int*)(settings + 0x20);
            const int needed = distance - 13;

            if (step < needed)
                step = needed;

            *(int*)(settings + 0x20) = step;
        }
    }

    if (g_result->slotFlag)
    {
        // цель заблокирована - снимаем задержку и часть настроек
        *(uint16_t*)(settings + 0x5C) = 0;
        *(uint8_t*)(settings + 0x52) = 0;
        *(float*)(settings + 0x4C) = 0.f;
        *(uint64_t*)(settings + 0x14) = 0;
    }
}

// ============================================================================
//  выбор записи прицеливания
//
//  Готовит таблицу весов по радиусу, обходит записи и запускает их обработку
//  (в общем случае — через пул потоков). Возвращает итоговую запись.
// ============================================================================
void* PickAimEntry(void* list, void* extra)
{
    char* base = (char*)list;

    // обрезаем список
    {
        char* first = *(char**)(base + 224);
        if (first != *(char**)(base + 232))
            *(char**)(base + 232) = first;
    }

    void* ctxData   = *(void**)(base + 16);
    void* targetObj = *(void**)base;

    *(uint64_t*)(base + 248) = 0;

    ResetAimState();

    if (!targetObj || !ctxData || !g_aimOffsetA)
        return nullptr;

    void* inner = ReadPtrGuarded((char*)ctxData + g_aimOffsetA + 8);
    if (!IsValid(inner))
        return nullptr;

    const int   v8  = ReadInt((char*)inner + g_aimOffsetB);
    const int   v9  = ReadInt((char*)inner + g_aimOffsetC);
    const float v10 = ReadFloat((char*)inner + g_aimOffsetD);
    const float v11 = ReadFloat((char*)inner + g_aimOffsetE);
    const float v12 = ReadFloat((char*)inner + g_aimOffsetF);
    const float v13 = GetTickInterval(-g_latencyB);

    const uint16_t v14 = (g_aimOffsetG && g_aimOffsetH && g_aimOffsetI)
                             ? ReadWord((char*)ctxData + g_aimOffsetG + g_aimOffsetI
                                        + g_aimOffsetH)
                             : 0;

    const int v15 = g_aimOffsetJ ? (int)Readable((char*)targetObj + g_aimOffsetJ) : 0;

    // ------------------------------------------------ параметры в структуру
    *(uint16_t*)(base + 282) = v14;
    *(int*)(base + 288)      = *(int*)(base + 204);
    *(int*)(base + 292)      = *(int*)(base + 172);
    *(float*)(base + 264)    = v10;
    *(float*)(base + 272)    = v12;
    *(int*)(base + 284)      = v15;
    *(float*)(base + 268)    = v11;
    *(float*)(base + 276)    = v13 * 0.0020000001f;
    *(int*)(base + 256)      = v8;
    *(int*)(base + 260)      = v9;

    *(uint64_t*)(base + 296) = (uint64_t)targetObj;

    // ---------------------------------------------- таблица весов по радиусу
    {
        const int have = (int)((*(uint64_t*)(base + 312) - *(uint64_t*)(base + 304)) / 520);
        const int need = (int)((*(uint64_t*)(base + 64)  - *(uint64_t*)(base + 56))  / 96);

        if (have < need)
            ReserveWeightTable(base + 304, need);
    }

    int count = 0;

    for (char* entry = *(char**)(base + 56); entry != *(char**)(base + 64); entry += 96)
    {
        const float radius = *(float*)(entry + 28);

        char* slot = nullptr;

        for (int i = 0; i < count; ++i)
        {
            char* candidate = *(char**)(base + 304) + 520 * i;

            if (*(float*)(candidate + 512) == radius)
            {
                slot = candidate;
                break;
            }
        }

        if (!slot)
        {
            slot = *(char**)(base + 304) + 520 * count;
            ++count;

            if (*(float*)(slot + 512) != radius)
            {
                *(float*)(slot + 512) = radius;

                const uint32_t hash = HashFloat((uint32_t)(520 * (count - 1)));
                *(uint32_t*)(slot + 516) = hash;

                // 64 пары коэффициентов, помноженные на радиус
                for (int k = 0; k < 64; ++k)
                {
                    const float* table = g_weightTable + 2 * k;

                    *(float*)(slot + 8 * k + 0) = radius * table[0];
                    *(float*)(slot + 8 * k + 4) = radius * table[1];
                }
            }
        }

        *(void**)(entry + 32) = slot;
    }

    // ------------------------------------------------ обработка всех записей
    {
        char* end = *(char**)(base + 40);

        for (char* it = *(char**)(base + 32); it != end; it += 208)
            ProcessAimEntry(base, it, extra);
    }

    // ------------------------------------------------------------- финал
    char* first = *(char**)(base + 224);
    const int total = (int)((*(uint64_t*)(base + 232) - (uint64_t)first) / 88);

    if (total == 1)
    {
        SelectAimEntry(base, first);
        return FinishAimList(base);
    }

    if (total > 0)
    {
        int limit = -1;

        if (total < 33)
        {
            void* pool = GetThreadPool();

            const int workers = pool ? GetPoolWorkers(pool) : 1;

            if (workers > total)
                limit = total;
        }

        void* pool = GetThreadPool();

        if (!pool)
        {
            RunAimBatch(&g_aimBatch, 0);
            return FinishAimList(base);
        }

        DispatchAimBatch(pool, &g_aimBatch, limit, 0, 2);
    }

    return FinishAimList(base);
}

// ============================================================================
//  фиксация выстрела
//
//  Вычищает из записи уже отработанные точки, ставит кнопку 0x800 и добавляет
//  запись с флагом 2048. Всегда возвращает false — выстрел считается
//  отправленным, пересчёт больше не нужен.
// ============================================================================
bool CommitShot(SolveContext* ctx)
{
    ShotPos* shot = *(ShotPos**)((char*)ctx + 4464);

    // выбрасываем точки, помеченные в +64 или +32
    {
        char* begin = *(char**)((char*)shot + 56);
        char* end   = *(char**)((char*)shot + 64);

        if (begin != end)
        {
            char* dst = begin;

            for (char* it = begin; it != end; it += 72)
            {
                if (*(uint8_t*)(it + 64))
                    continue;

                if (*(uint8_t*)(it + 32))
                    continue;

                memcpy(dst, it, 72);
                dst += 72;
            }

            if (dst != end)
                *(char**)((char*)shot + 64) = dst;
        }
    }

    // жмём 0x800
    {
        uint64_t* buttons = *(uint64_t**)((char*)ctx + 4472);
        *buttons |= 0x800;
    }

    ShotPos entry{};
    entry.angle = 0.f;
    *(uint64_t*)((char*)&entry + 8) = 2048;
    *(uint8_t*)((char*)&entry + 16) = 1;

    AddShot(entry);

    return false;
}

// ============================================================================
//  выбор лучшей точки
//
//  Отбрасывает цели по дистанции и FOV, прогоняет solve-проход по трём слотам
//  и выбирает запись с лучшим hit chance.
// ============================================================================
bool PickBest(SolveContext* ctx)
{
    if (g_shotState[0] != 0.f)
        MARKER(2);

    if (!*(uint8_t*)((char*)ctx + 81))
        return false;

    if (*(uint16_t*)((char*)ctx + 4448) == 31)
        return false;

    // максимальная дистанция: (fov * 0.34)^2
    const float fov     = *(float*)((char*)ctx + 2780) * 0.34f;
    const float maxDist = fov * fov;

    if (g_shotState[0] != 0.f)
        g_shotState[6] = maxDist;

    const float dx = *(float*)((char*)ctx + 2484);
    const float dy = *(float*)((char*)ctx + 2488);
    const float distSqr = dx * dx + dy * dy;

    if (*(uint8_t*)((char*)ctx + 2576) & 1)
    {
        if (*(int*)((char*)ctx + 3116) == -1 || !*(uint8_t*)((char*)ctx + 84))
        {
            if (g_shotState[0] != 0.f)
                MARKER(4);

            if (maxDist > distSqr)
                return false;

            if (g_shotState[0] != 0.f)
                MARKER(5);

            if (*(float*)((char*)ctx + 2988) == 1.f)
                return false;
        }
    }
    else
    {
        if (g_shotState[0] != 0.f)
            MARKER(3);

        if (maxDist > distSqr)
            return false;
    }

    // ---------------------------------------------------------- solve-проход
    if (*(uint8_t*)((char*)ctx + 196)
        || *(int*)((char*)ctx + 12608) != *(int*)((char*)ctx + 2820))
    {
        FillCmdRecord((char*)ctx + 10624, (char*)ctx + 2472, 17, nullptr);

        StopParams params = MakeStopParams(ctx);
        SolveStopPath(&params, (char*)ctx + 10624, (char*)ctx + 8504, 0, 32, maxDist);

        *(int*)((char*)ctx + 12608) = *(int*)((char*)ctx + 2820);
    }

    const float adx = *(float*)((char*)ctx + 10636);
    const float ady = *(float*)((char*)ctx + 10640);
    const float aimDistSqr = adx * adx + ady * ady;

    const ShotSlot aimSlot = ShotSlotIndex(*(int*)((char*)ctx + 10972));
    const bool tooFar  = (aimSlot.index == *(int*)((char*)ctx + 11476))
                             ? aimSlot.frac < *(float*)((char*)ctx + 11480)
                             : aimSlot.index < *(int*)((char*)ctx + 11476);

    const bool marker = (g_shotState[0] != 0.f);
    const int  flag1  = *(int*)((char*)ctx + 10728) & 1;

    if (marker)
    {
        *(uint8_t*)&g_shotState[7] = 1;

        const float aimLen = (aimDistSqr < 0.f) ? FastSqrt(aimDistSqr) : sqrtf(aimDistSqr);

        *(float*)&g_shotInfo[0]  = aimLen;
        *(int*)&g_shotInfo[4]    = *(int*)((char*)ctx + 10912) - *(int*)((char*)ctx + 2760);
        g_shotInfo[8]            = !tooFar;
        g_shotInfo[9]            = (char)flag1;

        MARKER(6);
    }

    if (*(int*)((char*)ctx + 10912) <= *(int*)((char*)ctx + 2760))
        return false;

    if (marker)
        MARKER(7);

    if (aimDistSqr > maxDist)
        return false;

    if (marker)
        MARKER(8);

    if (tooFar)
        return false;

    const bool stopCandidate = *(uint8_t*)((char*)ctx + 118)
                            && ((*(uint8_t*)((char*)ctx + 2576) & 1) == 0
                                || *(int*)((char*)ctx + 3116) != -1)
                            && flag1;

    if (marker)
        MARKER(9);

    if (!flag1 && !*(uint8_t*)((char*)ctx + 83))
        return false;

    if (marker)
        MARKER(10);

    if (stopCandidate && !*(uint8_t*)((char*)ctx + 84))
        return false;

    // ------------------------------------------------------ очистка списков
    *(uint16_t*)((char*)ctx + 7120) = 0;
    *(uint64_t*)((char*)ctx + 7288) = 0;
    *(uint64_t*)((char*)ctx + 7040) = 0;
    *(uint16_t*)((char*)ctx + 7320) = 0;

    TruncateList((char*)ctx + 7072, (char*)ctx + 7080);
    TruncateList((char*)ctx + 7096, (char*)ctx + 7104);
    TruncateList((char*)ctx + 7264, (char*)ctx + 7272);

    ClearShotList((char*)ctx + 7040);

    // --------------------------------------------------- сбор точек по слотам
    {
        AimQuery query;

        query.flag0   = *(uint8_t*)((char*)ctx + 64);
        query.tickNow = *(int*)((char*)ctx + 2820);
        query.list    = &g_scanData;
        query.index   = *(int*)((char*)ctx + 2316);
        query.frac    = *(float*)((char*)ctx + 2320);
        query.mode    = *(int*)((char*)ctx + 96);
        query.flag1C  = *(uint8_t*)((char*)ctx + 8);
        query.flag1D  = *(uint8_t*)((char*)ctx + 86);
        query.flag1E  = *(uint8_t*)((char*)ctx + 208);
        query.field20 = *(int*)((char*)ctx + 4);
        query.field24 = *(int*)((char*)ctx + 56);

        CollectAimPoints(&query, (char*)ctx + 10624, nullptr, nullptr, (char*)ctx + 7096);
    }

    SortAimPoints(ctx, (char*)ctx + 7040, (char*)ctx + 10624);

    // помечаем три слота текущим тиком
    const int slotCount = g_aimSlotCount;

    for (int i = 0; i < slotCount && i < 3; ++i)
        *(int*)((char*)&g_aimSlots + 4104 * i + 4084) = *(int*)((char*)ctx + 10972) + 1;

    for (int j = 0; j < slotCount && j < 3; ++j)
    {
        char* slot = (char*)&g_aimSlots + 4104 * j;

        if (!*(void**)(slot + 8))
            continue;

        int delta = 0;
        if (*(int*)((char*)ctx + 132) - *(int*)((char*)ctx + 32) > 0)
            delta = *(int*)((char*)ctx + 132) - *(int*)((char*)ctx + 32);

        ValidateAimSlot(slot, *(int*)((char*)ctx + 10972) - delta);
    }

    FinalizeAimPoints(ctx, (char*)ctx + 7040);

    void* best = PickAimEntry((char*)ctx + 7040, (char*)ctx + 7888);

    // запасной угол, если список пуст
    const float fallbackAngle = (*(uint64_t*)((char*)ctx + 7096) == *(uint64_t*)((char*)ctx + 7104))
                                    ? -1.f
                                    : *(float*)(*(uint64_t*)((char*)ctx + 7096) + 28);

    bool marker2 = (g_shotState[0] != 0.f);

    if (marker2)
    {
        *(int*)&g_shotInfo[4]  = *(int*)((char*)ctx + 11504);
        *(int*)&g_shotInfo[8]  = *(int*)((char*)ctx + 12560);
        *(int*)&g_shotInfo[12] = *(int*)&fallbackAngle;
        g_shotInfo[2]          = 1;
    }

    // ------------------------------------------------------------ отбор
    if (!best)
        goto no_best;

    {
        void* bestTarget = *(void**)((char*)best + 16);
        if (!bestTarget)
            goto no_best;

        if (marker2)
        {
            // пишем hitchance и урон
            *(int*)&g_bestHit[4]  = *(int*)((char*)best + 68);
            *(int*)&g_bestHit[8]  = *(int*)((char*)ctx + 76);
            *(int*)&g_bestHit[12] = *(int*)((char*)best + 12);

            g_bestDamage = *(int*)((char*)bestTarget + 108);
            g_bestHit[0] = 1;
        }

        // нужный урон должен быть не больше расчётного
        if (*(float*)((char*)bestTarget + 108) <= *(float*)((char*)best + 12))
        {
            if (*(float*)((char*)ctx + 76) > *(float*)((char*)best + 68))
                best = nullptr;
        }
        else
        {
            goto no_best;
        }
    }

    if (marker2)
        MARKER(11);

    if (!best)
        return false;

    // если задержка выстрела не включена либо уже прострелили — на выход
    if ((!*(uint8_t*)((char*)ctx + 92) && !*(uint8_t*)((char*)ctx + 93))
        || stopCandidate
        || *(float*)((char*)best + 72) >= 1.f)
    {
        goto accept;
    }

    // финальная проверка с задержкой
    {
        FillCmdRecord((char*)ctx + 8640, (char*)ctx + 2472, 17, nullptr);

        StopParams params = MakeStopParams(ctx);

        const int budget = *(int*)((char*)ctx + 88);
        const int tick   = *(int*)((char*)ctx + 10972) - *(int*)((char*)ctx + 2760);

        SolveStopPath(&params, (char*)ctx + 8640, (char*)ctx + 8504, budget, tick, -1.f);
    }

    if (g_shotState[0] != 0.f)
        MARKER(12);

    if (FinalShotCheck(ctx, best, (char*)ctx + 8640, 0))
        return false;

    marker2 = (g_shotState[0] != 0.f);

accept:
    if (marker2)
        MARKER(13);

    FinishShotSolve(ctx);
    return true;

no_best:
    best = nullptr;
    goto accept;
}

// ============================================================================
//  резервный просчёт
//
//  Берёт лучшую цель и строит две точки прицеливания со смещением вбок —
//  на случай, если основной просчёт не дал результата.
// ============================================================================
void FallbackSolve(SolveContext* ctx)
{
    const int targetCount = (int)(g_targetsEnd - g_targets);

    if ((int)g_best[0] < 0 || (int)g_best[0] >= targetCount)
        return;

    TargetEntry* target = &g_targets[g_best[0]];

    if (target->state != 2)
        return;

    // ---------------------------------------------------------- очистка
    *(uint16_t*)((char*)ctx + 7120) = 0;
    *(uint64_t*)((char*)ctx + 7288) = 0;
    *(uint64_t*)((char*)ctx + 7040) = 0;
    *(uint16_t*)((char*)ctx + 7320) = 0;

    TruncateList((char*)ctx + 7072, (char*)ctx + 7080);
    TruncateList((char*)ctx + 7096, (char*)ctx + 7104);
    TruncateList((char*)ctx + 7264, (char*)ctx + 7272);

    RebuildAimList(ctx, (char*)ctx + 7040, (char*)ctx + 2472);

    // ------------------------------------------------------- направление
    const float dirX = *(float*)((char*)ctx + 152);
    const float dirY = *(float*)((char*)ctx + 156);
    const float dirZ = *(float*)((char*)ctx + 160);

    const float lengthSqr = dirX * dirX + dirY * dirY + dirZ * dirZ;
    const float length    = (lengthSqr < 0.f) ? FastSqrt(lengthSqr) : sqrtf(lengthSqr);

    float nx, ny, nz;

    if (length <= 0.00001f)
    {
        nx = dirY;
        nz = dirZ;
        ny = dirY;
    }
    else
    {
        const float inv = 1.f / length;
        nx = dirX * inv;
        ny = dirY * inv;
        nz = dirZ * inv;
    }

    // боковой перпендикуляр к направлению
    float sideX = 0.f, sideY = 0.f, sideZ = 0.f;

    if (fabsf(nx) >= 0.001f || fabsf(ny) >= 0.001f)
    {
        const float px = ny;
        const float py = -nx;
        const float pz = 0.f;

        const float perpSqr = px * px + py * py + pz * pz;

        if (perpSqr == 0.f)
        {
            sideX = sideY = sideZ = 0.f;
        }
        else
        {
            const float inv = 1.f / sqrtf(perpSqr);
            sideX = px * inv;
            sideY = py * inv;
            sideZ = pz * inv;
        }
    }

    // ------------------------------------------------------- параметры
    const float radius   = *(float*)((char*)ctx + 3224);
    const float angleMax = 32.f - *(float*)((char*)ctx + 2988) * 12.f;
    const float angleMin = *(uint8_t*)((char*)ctx + 2984) ? 54.f : 64.f;

    char record[96];
    CopyShotRecord(record, (char*)ctx + 2472);

    // шаблон точки
    char point[192] = {};
    *(float*)&point[8]   = radius;
    *(float*)&point[0]   = -0.25f;
    *(float*)&point[4]   = -0.25f;
    *(float*)&point[12]  = 16.f;
    *(float*)&point[16]  = 16.f;
    *(float*)&point[20]  = angleMin;
    *(int*)&point[32]    = 2;

    char extra[192];
    memset(extra, 0, 192);
    *(int*)&extra[0]  = 0x3F800000;                     // 1.0f
    *(int*)&extra[8]  = -1;
    *(int*)&extra[12] = -1;

    // ------------------------------------------------- две точки: ±смещение
    float offset[3];

    offset[0] = -sideX;
    offset[1] = -sideY;
    offset[2] = -sideZ;

    AddAimPoint(ctx, (char*)ctx + 7040, point, record, extra, angleMax, radius, target, offset);

    offset[0] = sideX;
    offset[1] = sideY;
    offset[2] = sideZ;

    AddAimPoint(ctx, (char*)ctx + 7040, point, record, extra, angleMax, radius, target, offset);

    // -------------------------------------------- перенос из цели, если есть
    if (*(uint8_t*)(target + 40))
    {
        char source[208];

        void* from = GetTargetData(source, target);
        memcpy(&source[0], (char*)from, 208);

        AppendTargetData((char*)ctx + 7072, source);
    }

    // ---------------------------------------------------------- финализация
    *(int*)((char*)ctx + 7064)  = *(int*)((char*)ctx + 2820);
    *(uint64_t*)((char*)ctx + 7040) = *(uint64_t*)((char*)ctx + 2800);
    *(uint64_t*)((char*)ctx + 7056) = *(uint64_t*)((char*)ctx + 4432);

    *(float*)((char*)ctx + 7200) = 0.f;
    *(float*)((char*)ctx + 7204) = -0.31415927f;        // -1104276357

    *(uint64_t*)((char*)ctx + 7124) = *(uint64_t*)((char*)ctx + 164);
    *(int*)((char*)ctx + 7132)      = *(int*)((char*)ctx + 172);
    *(uint64_t*)((char*)ctx + 7136) = *(uint64_t*)((char*)ctx + 152);
    *(int*)((char*)ctx + 7144)      = *(int*)((char*)ctx + 160);

    {
        const int* shot = *(const int**)((char*)ctx + 4464);

        *(int*)((char*)ctx + 7160) = shot[0];
        *(int*)((char*)ctx + 7164) = shot[1];
        *(int*)((char*)ctx + 7168) = shot[2];
    }

    FinalizeFallback(ctx, (char*)ctx + 7040, *(void**)((char*)ctx + 2800), 1);
    PickAimEntry((char*)ctx + 7040, (char*)ctx + 7888);
}

// ============================================================================
//  quick stop
//
//  Планировщик остановки: берёт запись тика, строит кандидаты точки остановки,
//  прогоняет их через solve-проход и решает, останавливаться или нет.
// ============================================================================
void QuickStop(SolveContext* ctx)
{
    // глобальный выключатель
    if (g_stopDisabled)
        return;

    if (!*(uint8_t*)((char*)ctx + 104))
        return;

    if (*(uint8_t*)((char*)ctx + 2584) != 2)
        return;

    void* weaponData = *(void**)((char*)ctx + 4440);
    if (!weaponData)
        return;

    // только авто-оружие (тип 1..6) либо автснайпер (id 31)
    const int type = WeaponInt(weaponData, off_m_WeaponType);

    if (!((unsigned)(type - 1) < 6 || *(uint16_t*)((char*)ctx + 4448) == 31))
        return;

    // если у оружия уже есть активный выстрел — стоп не нужен
    {
        void* holder = *(void**)((char*)ctx + 4456);

        if (holder)
        {
            void* inner = *(void**)((char*)holder + 64);

            if (IsValid(inner) && ReadInt((char*)inner + 104) != 0)
                return;
        }
    }

    const int slot = *(int*)((char*)ctx + 188);

    const int entryIndex = GetStopEntryIndex(slot);
    if (entryIndex < 0)
        return;

    if (!g_tickFn)
        return;

    if (!IsValid(*(void**)((char*)ctx + 2808)) || entryIndex <= 0)
        return;

    void* tickTable = ((void* (*)())g_tickFn)();
    if (!IsValid(tickTable))
        return;

    if (!g_tickFnCount)
        return;

    void* tickEntry = (char*)tickTable + g_tickFnStride * (entryIndex % g_tickFnCount);

    if (!IsValid(tickEntry) || *(uint64_t*)((char*)tickEntry + 8) != (uint64_t)entryIndex)
        return;

    void* inner = ReadPtr((char*)tickEntry + 64);

    // если у записи уже есть активный выстрел — выходим
    if (IsValid(inner) && ReadInt((char*)inner + 104) != 0)
        return;

    const int recordIndex = entryIndex - 1;
    CmdRecord* record = GetRecord(recordIndex);
    if (!record)
        return;

    // запись должна быть «живой» и от того же тика
    if (*(uint8_t*)((char*)record + 0x80) != 2)
        return;

    if (*(int*)((char*)record + 0x35C) != *(int*)((char*)ctx + 3316))
        return;

    // ------------------------------------------------------------ кандидаты
    char request[128] = {};
    float lateral[2] = { 0.f, 0.f };
    float candidateA = 0.f;
    float candidateB = 0.f;

    ShotList candidates{};

    float dirX = 0.f;
    float dirY = 0.f;
    float lenSqr = 0.f;
    int   hitCount = 0;
    int   budget = 0;

    BuildStopRequest(tickEntry, request, lateral);

    void* shot = *(void**)((char*)ctx + 4464);

    if (off_m_nZoomLevels && ReadInt((char*)weaponData + off_m_nZoomLevels) > 0)
    {
        // уже жмём 0x800 где-то в цепочке — выходим
        uint64_t* buttonsPtr = *(uint64_t**)((char*)ctx + 4472);
        const uint64_t buttons = buttonsPtr ? *buttonsPtr : 0;

        const uint32_t shotFlags = *(uint32_t*)((char*)shot + 40)
                                 | *(uint32_t*)((char*)shot + 48);

        if ((shotFlags | (uint32_t)candidateA | (uint32_t)candidateB | (uint32_t)buttons) & 0x800)
            goto cleanup;

        if (HasFlagInList(*(char**)((char*)shot + 56), *(char**)((char*)shot + 64), 0x800))
            goto cleanup;

        if (HasFlagInList(candidates.begin, candidates.end, 0x800))
            goto cleanup;
    }

    // ------------------------------------------------------- первый кандидат
    *(uint64_t*)((char*)shot + 40) |= 0x618;

    ComputeStopDir(request, lateral, &candidateA, &candidateB);
    ComputeStopDir((char*)ctx + 8616, (char*)ctx + 8548, &lateral[0], &lateral[1]);

    dirX = lateral[0];
    dirY = lateral[1];

    lenSqr = dirX * dirX + dirY * dirY;
    if (lenSqr > 1.f)
    {
        const float inv = 1.f / sqrtf(lenSqr);
        dirX *= inv;
        dirY *= inv;
    }

    hitCount = GetHitCount(slot);
    if (hitCount < 0)
        hitCount = 0;

    budget = GetStopBudget() - hitCount;

    if (budget <= 0)
        goto cleanup;

    if ((*(uint32_t*)((char*)shot + 40) ^ (uint32_t)candidateA) & 0x2827)
        goto cleanup;

    {
        uint64_t* buttonsPtr = *(uint64_t**)((char*)ctx + 4472);
        const uint64_t buttons = buttonsPtr ? *buttonsPtr : 0;

        if (buttons & 0x2827)
            goto cleanup;
    }

    // ------------------------------------------------------ прогон кандидатов
    if ((*(uint8_t*)((char*)ctx + 2576) & 3) == 2
        && *(float*)((char*)ctx + 2492) < 0.f
        && (*(uint8_t*)((char*)ctx + 4488) & 2) == 0)
    {
        StopParams params;
        params.slot     = slot;
        params.hitCount = hitCount;
        params.budget   = budget;
        params.ctx      = ctx;

        params.flag82   = *(uint8_t*)((char*)ctx + 82);
        params.flag81   = *(uint8_t*)((char*)ctx + 81);
        params.flag84   = *(uint8_t*)((char*)ctx + 84);
        params.flag118  = *(uint8_t*)((char*)ctx + 118);
        params.flag119  = *(uint8_t*)((char*)ctx + 119);
        params.flag208  = *(uint8_t*)((char*)ctx + 208);
        params.mode     = *(int*)((char*)ctx + 96);

        params.callback = (void* (*)())OnScanTick;
        if (*(void**)((char*)ctx + 8632))
            params.callback = *(void* (**)())((char*)ctx + 8632);

        if (*(uint8_t*)((char*)ctx + 8496))
        {
            params.extra0 = (char*)ctx + 8368;
            params.extra1 = (char*)ctx + 224;
        }
        else
        {
            params.extra0 = nullptr;
            params.extra1 = nullptr;
        }

        InitStopPath((void*)&g_stopPath, (char*)ctx + 2472);
        ResetStopPath();

        if (hitCount)
        {
            char entryBuf[64];
            BuildStopEntry(entryBuf, request);

            SolveStopPath((void*)&params, (void*)&g_stopPath, entryBuf, budget, budget, -1.f);

            if (g_stopPathAbort)
            {
                FinishStop((void*)&params, 2, -1);
                goto cleanup;
            }

            if (g_stopPathSkip & 1)
            {
                *(uint8_t*)((char*)ctx + 14636) = 1;
                FinishStop((void*)&params, 1, -1);
                goto cleanup;
            }

            InitStopPath((void*)&g_stopPath, (char*)ctx + 2472);
            ResetStopPath();
        }

        // второй проход — уже с учётом попадания
        {
            char entryBuf2[64];
            BuildStopEntry(entryBuf2, (char*)ctx + 8504);

            const int count = (hitCount != 0) + budget;

            SolveStopPath((void*)&params, (void*)&g_stopPath, entryBuf2, count, count, -1.f);

            if (g_stopPathAbort)
            {
                FinishStop((void*)&params, (hitCount != 0) - 1, 2);
                goto cleanup;
            }

            if (g_stopPathSkip & 1)
            {
                *(uint8_t*)((char*)ctx + 14636) = (hitCount == 0);
                FinishStop((void*)&params, (hitCount != 0) - 1, 1);
                goto cleanup;
            }

            FinishStop((void*)&params, (hitCount != 0) - 1, 0);
        }
    }

    // ------------------------------------------------------------- финальная
    {
        const float dx = dirX - candidateA;
        const float dy = dirY - candidateB;

        if (dx * dx + dy * dy <= 0.1f
            && ShotCount(candidates) <= 1
            && ShotCount(*reinterpret_cast<ShotList*>(shot)) <= 1)
        {
            *(uint8_t*)((char*)ctx + 14636) = 1;
        }
    }

cleanup:
    candidates.Release();
}

// ============================================================================
//  вариант стопа
//
//  Собирает точки по всем целям и слотам, прогоняет трассировки и выбирает
//  лучшую запись. Возвращает true, если точка найдена и применена.
// ============================================================================
bool StopVariant(SolveContext* ctx)
{
    // ---------------------------------------------------- сброс списков
    TruncateList((char*)ctx + 0, (char*)ctx + 0);

    if (*(uint8_t*)((char*)ctx + 196)
        || *(int*)((char*)ctx + 572) != *(int*)((char*)ctx + 2820))
    {
        ShotBookkeeping((char*)ctx + 208);

        *(uint64_t*)((char*)ctx + 8360) = **(uint64_t**)((char*)ctx + 4472);

        FillCmdRecord((char*)ctx + 224, (char*)ctx + 2472, 0, nullptr);

        char request[128];
        void* res = BuildSolveRequest(request, ctx, *(void**)((char*)ctx + 4464),
                                      *(void**)((char*)ctx + 8360), &g_shotSlot);

        QueueSolveResult((char*)ctx + 8368, res);
        MergeSolve((char*)ctx + 224, (char*)ctx + 8368);

        *(uint8_t*)((char*)ctx + 8496) = 1;
    }

    // ------------------------------------------------------- запрос точек
    AimQuery query;

    query.flag0   = *(uint8_t*)((char*)ctx + 64);
    query.tickNow = *(int*)((char*)ctx + 2820);
    query.list    = &g_scanData;
    query.index   = *(int*)((char*)ctx + 2316);
    query.frac    = *(float*)((char*)ctx + 2320);
    query.mode    = *(int*)((char*)ctx + 96);
    query.flag1C  = *(uint8_t*)((char*)ctx + 8);
    query.flag1D  = *(uint8_t*)((char*)ctx + 86);
    query.flag1E  = *(uint8_t*)((char*)ctx + 208);
    query.field20 = *(int*)((char*)ctx + 4);
    query.field24 = *(int*)((char*)ctx + 56);

    CollectAimPoints(&query, (char*)ctx + 224, nullptr, nullptr, g_aimListA);
    CollectAimPoints(&query, (char*)ctx + 224, nullptr, (void*)1,  g_aimListB);

    // -------------------------------------------------- обход всех целей
    for (char** it = (char**)g_targetList; it != (char**)g_targetListEnd; ++it)
    {
        char* target = *it;
        if (!target)
            continue;

        if (!*(uint8_t*)(target + 40))
            continue;

        void* data = *(void**)(target + 32);
        if (!data)
            continue;

        char aimEntry[48] = {};
        InitAimEntry(aimEntry, *(void**)(target + 8));

        for (int slot = 0; slot < 2; ++slot)
        {
            char* listBegin = *(char**)&g_aimListA[slot * 24];
            char* listEnd   = *(char**)&g_aimListA[slot * 24 + 8];

            for (char* point = listBegin; point != listEnd; point += 24)
            {
                SetAimTarget((char*)ctx + 224, data, point);

                for (char** hit = (char**)g_aimHits; hit != (char**)g_aimHitsEnd; ++hit)
                {
                    char* entry = *hit;

                    if (g_traceFnA)
                        TraceRay((void*)g_traceFnA, *(void**)(target + 8), entry + 20);

                    if (g_traceFnB)
                        TraceRay((void*)g_traceFnB, *(void**)(target + 8), entry + 68);

                    TraceAim(*(void**)(target + 8), entry + 32, entry + 44);

                    // дескриптор списка попаданий
                    g_aimDescriptorCount = (int)((*(uint64_t*)(entry + 160)
                                                - *(uint64_t*)(entry + 152)) >> 5);
                    g_aimDescriptorData  = *(uint64_t*)(entry + 152);
                    g_aimDescriptorSize  = g_aimDescriptorCount;
                    g_aimDescriptorFlag  = 0x80000000;

                    if (*(void**)(target + 8) && g_entityOffset)
                    {
                        void* entity = ReadPtrGuarded((char*)*(void**)(target + 8) + g_entityOffset);

                        if (entity)
                        {
                            void* resolved = ResolveEntity(entity);

                            if (resolved)
                                AppendEntry((char*)resolved + 1088, &g_aimDescriptor);
                        }
                    }

                    StoreAimResult(aimEntry, data, entry, point, slot);
                }
            }

            // финальная трассировка по накопленному
            if (g_traceFnA)
                TraceRay((void*)g_traceFnA, *(void**)(target + 8), aimEntry + 0);

            if (g_traceFnB)
                TraceRay((void*)g_traceFnB, *(void**)(target + 8), aimEntry + 20);

            TraceAim(*(void**)(target + 8), aimEntry + 12, aimEntry + 32);

            if (*(void**)(target + 8) && g_entityOffset)
            {
                void* entity = ReadPtrGuarded((char*)*(void**)(target + 8) + g_entityOffset);

                if (entity)
                {
                    void* resolved = ResolveEntity(entity);

                    if (resolved)
                        AppendEntry((char*)resolved + 1088, &aimEntry[0]);
                }
            }
        }
    }

    // ---------------------------------------------------------- выбор лучшей
    char* best = nullptr;

    if (g_aimResults == g_aimResultsEnd)
        return false;

    for (char* it = g_aimResults; it != g_aimResultsEnd; it += 48)
    {
        if (!best)
        {
            best = it;
            continue;
        }

        const bool better =
            (!*(uint8_t*)(it + 16) || *(uint8_t*)(best + 16))
            && *(uint32_t*)(it + 12) <= *(uint32_t*)(best + 12)
            && (!*(uint8_t*)(*(void**)(it + 24) ? *(char**)(it + 24) : (char*)"" + 20)
                || *(uint8_t*)(*(char**)(best + 24) + 20))
            && (*(uint8_t*)(it + 17) != 1 || *(uint8_t*)(best + 17));

        if (better)
            best = it;
    }

    if (!best)
        return false;

    // нужный режим должен быть разрешён
    {
        const bool knife = (*(uint8_t*)(best + 17) == 1);
        const bool blocked = knife ? (*(uint8_t*)((char*)ctx + 116) == 0)
                                   : (*(uint8_t*)((char*)ctx + 115) == 0);

        if (blocked)
            return false;
    }

    // ------------------------------------------------ данные точки и попадания
    char* pointData = *(char**)(best + 24);
    char* hitData   = *(char**)(best + 40);

    char aimData[56] = {};
    *(uint64_t*)&aimData[0]  = *(uint64_t*)(pointData + 0);
    *(int*)&aimData[8]       = *(int*)(pointData + 8);
    *(uint8_t*)&aimData[16]  = *(uint8_t*)(pointData + 20);
    *(uint64_t*)&aimData[24] = *(uint64_t*)(pointData + 48);
    *(uint8_t*)&aimData[32]  = *(uint8_t*)(pointData + 56);
    *(int*)&aimData[36]      = *(int*)(pointData + 72);
    *(uint8_t*)&aimData[40]  = *(uint8_t*)(pointData + 76);

    char hitInfo[64] = {};

    *(uint64_t*)&hitInfo[0]  = *(uint64_t*)(hitData + 0);
    *(uint8_t*)&hitInfo[8]   = *(uint8_t*)(hitData + 124);
    *(int*)&hitInfo[12]      = *(int*)(hitData + 96);
    *(int*)&hitInfo[16]      = *(int*)(hitData + 100);
    *(int*)&hitInfo[20]      = *(int*)(hitData + 104);
    *(uint8_t*)&hitInfo[24]  = 1;
    *(int*)&hitInfo[32]      = *(int*)(hitData + 108);
    *(int*)&hitInfo[36]      = *(int*)(hitData + 112);
    *(int*)&hitInfo[40]      = *(int*)(hitData + 116);
    *(uint8_t*)&hitInfo[44]  = 1;
    *(int*)&hitInfo[56]      = *(int*)(hitData + 120);

    **(uint64_t**)((char*)ctx + 4472) = *(uint64_t*)((char*)ctx + 8360);

    {
        const bool knife = (*(uint8_t*)(best + 17) == 1);

        ApplyShot((char*)ctx + 208, aimData, best, hitInfo, knife, &g_shotSlot);

        if (knife)
            **(uint64_t**)((char*)ctx + 4472) &= ~1ull;
    }

    *(uint8_t*)((char*)ctx + 121) = 1;

    ApplyFinalAim2((char*)ctx + 208, *(void**)(best + 32), (void*)((char*)ctx + 4464));

    // ------------------------------------------------------ запись результата
    char record[512] = {};

    *(int*)&record[0]   = *(int*)((char*)ctx + 188);
    *(int*)&record[4]   = *(int*)((char*)ctx + 2820);
    *(int*)&record[8]   = *(int*)((char*)*(void**)(best + 32) + 16);
    *(int*)&record[12]  = *(int*)((char*)ctx + 1068);
    *(uint8_t*)&record[16] = 1;

    *(uint64_t*)&record[20] = *(uint64_t*)pointData;
    *(int*)&record[28]      = *(int*)(pointData + 8);
    *(uint64_t*)&record[32] = *(uint64_t*)best;
    *(int*)&record[40]      = *(int*)(best + 8);
    *(uint64_t*)&record[44] = *(uint64_t*)hitData;

    CopyAimRecord(&record[48], hitData);

    // ---------------------------------------------------------- время точки
    {
        int   delta = *(int*)((char*)ctx + 2820) - *(int*)hitData;
        float frac  = 0.f - *(float*)(hitData + 4);

        if (frac < 0.f)
        {
            frac += 1.f;

            if (frac >= 1.f)
                frac = 0.f;
            else
                --delta;
        }

        *(float*)&record[84] = (float)delta + frac;
    }

    {
        float pair[2];
        const float frac = GetTimeFraction(pair);
        const float base = *(float*)&pair[0];

        int   delta = (int)frac - (int)base;
        float sub   = 0.f;

        if (frac < 0.f)
        {
            if (frac + 1.f < 1.f)
                sub = *(float*)&pair[0] - 1.f;
        }

        (void)delta;
        (void)sub;

        // разница по времени
        const float shotTime = GetShotTime((char*)ctx + 224);

        (void)shotTime;
    }

    *(int*)&record[88] = 1;
    *(float*)&record[92] = (float)*(int*)(best + 12);

    // ---------------------------------------------------------- публикация
    {
        struct { int a; float b; } entry;

        entry.a = *(int*)((char*)*(void**)(best + 32) + 16);
        entry.b = (float)*(int*)(best + 12);

        if (*(uint64_t*)((char*)ctx + 7896) == *(uint64_t*)((char*)ctx + 7904))
            GrowAimList((char*)ctx + 7888, *(void**)((char*)ctx + 7896), &entry);
        else
        {
            *(uint64_t*)(*(uint64_t*)((char*)ctx + 7896)) = *(uint64_t*)&entry;
            *(float*)(*(uint64_t*)((char*)ctx + 7896) + 8) = entry.b;
            *(uint64_t*)((char*)ctx + 7896) += 12;
        }
    }

    {
        if (*(uint64_t*)((char*)ctx + 7920) == *(uint64_t*)((char*)ctx + 7928))
            GrowRecordList((char*)ctx + 7912, *(void**)((char*)ctx + 7920), record);
        else
        {
            CopyRecord(*(void**)((char*)ctx + 7920), record);
            *(uint64_t*)((char*)ctx + 7920) += 512;
        }
    }

    return true;
}

// ============================================================================
//  нож
// ============================================================================
void KnifeBot(int mode, uint64_t* cmd, uint64_t* buttons)
{
    if (mode == 2)
        return;

    const uint64_t flags5 = cmd[5];
    const uint64_t btn    = *buttons;

    // уже атакуем или прыгаем
    if ((btn | flags5) & 0x2001)
        return;

    if (g_local->clip <= 0)
        return;

    if (*(uint8_t*)((char*)g_local + 1940))
        return;

    const uint64_t flags6 = cmd[6];
    const int      tick   = g_local->tick;

    // сбрасываем два маркера в кольце выстрелов
    if (g_scanBegin != g_scanEnd)
    {
        g_shotMarker0 = ShotMarker{ 0, 1 };
        g_shotMarker1 = ShotMarker{ 0, 1 };
        g_shotDirty   = true;
    }

    const uint64_t cleared5 = flags5 & 0xFFFFFFFFFFFFD7FEull;

    cmd[5]   = cleared5;
    *buttons = btn & 0xFFFFFFFFFFFFF7FEull;

    // ещё не готовы бить
    if (!*(uint8_t*)((char*)g_local + 894))
    {
        const bool ready = (tick == g_local->tickB)
                               ? g_local->fracB > 0.f
                               : tick < g_local->tickB;

        if (!ready)
            cmd[5] = cleared5 | 1;

        return;
    }

    const int   attackTick = *(int*)((char*)g_local + 868);
    const float attackTime = *(float*)((char*)g_local + 872);

    const bool attackReady = (tick == attackTick)
                                 ? attackTime > 0.f
                                 : tick < attackTick;

    // окно удара открыто
    if (!attackReady && (attackTick == tick || (attackTick == tick - 1 && attackTime > 0.f)))
    {
        ShotPos swing{};
        swing.angle = 0.f;
        *(uint64_t*)((char*)&swing + 8) = 2048;
        *(uint8_t*)((char*)&swing + 16) = 1;
        AddShot(swing);

        cmd[5] &= ~1ull;
        cmd[5] |= 0x800ull;

        float time;
        if (attackTick == tick && attackTime == 0.f)
        {
            time = 1.f;
        }
        else
        {
            const float trimmed = (attackTime + 131072.f) - 131072.f;
            time = (attackTime >= trimmed) ? trimmed : fmaxf(0.f, trimmed - 0.015625f);
        }

        ShotPos hit{};
        hit.angle = time;
        *(uint64_t*)((char*)&hit + 8) = 1;
        *(uint8_t*)((char*)&hit + 16) = 0;
        AddShot(hit);

        return;
    }

    const int   secondTick = *(int*)((char*)g_local + 860);
    const float secondTime = *(float*)((char*)g_local + 864);

    const bool secondReady = (tick == secondTick)
                                 ? secondTime > 0.f
                                 : tick < secondTick;

    // окно второй атаки
    if (!secondReady && (secondTick == tick || (secondTick == tick - 1 && secondTime > 0.f)))
    {
        ShotPos a{};
        a.angle = 0.f;
        *(uint64_t*)((char*)&a + 8) = 2048;
        *(uint8_t*)((char*)&a + 16) = 0;
        AddShot(a);

        cmd[5] &= ~0x800ull;
        cmd[5] |= 1ull;

        ShotPos b{};
        b.angle = 0.984375f;
        *(uint64_t*)((char*)&b + 8) = 1;
        *(uint8_t*)((char*)&b + 16) = 1;
        AddShot(b);

        ShotPos c{};
        c.angle = 1.f;
        *(uint64_t*)((char*)&c + 8) = 1;
        *(uint8_t*)((char*)&c + 16) = 1;
        AddShot(c);

        return;
    }

    if (attackReady)
    {
        cmd[5]   = cleared5 | 1;
        cmd[6]   = flags6 | 1;
        *buttons = (btn & 0xFFFFFFFFFFFFF7FEull) | 1;
        return;
    }

    if (secondReady)
    {
        cmd[5]   = flags5 | 0x800ull;
        cmd[6]   = flags6 | 0x800ull;
        *buttons = (btn & 0xFFFFFFFFFFFFF7FEull) | 0x800;
    }
}

// ============================================================================
//  главная функция
// ============================================================================
namespace RageBot
{
    void Run(CUserCmd* cmd, uint64_t* pred);
}

void RageBot::Run(CUserCmd* cmd, uint64_t* pred)
{
    // ---------------------------------------------------------- инициализация
    if (!g_offsetsReady && g_schemaSystem)
        ResolveWeaponOffsets();

    if (!g_logReady)
    {
        g_logReady = true;
        StartLogThread();
    }

    g_result->field09 = 0;
    ResetWhy();

    if (!cmd || !g_moduleReady || !*pred)
        return;

    uint64_t buttons = cmd->buttons;

    // ------------------------------------------------------------- проверки
    void* pawn = g_local->pawn;
    const bool pawnValid = pawn
                        && off_m_nTickBase
                        && Readable((char*)pawn + off_m_nTickBase);

    g_rage->valid = pawnValid;

    if (!g_enabled
        || g_gameState == 2
        || (g_gameState == 1 && pawnValid))
    {
        g_result->active  = 0;
        g_result->field0A = 0;
        g_result->field0C = 0;
        return;
    }

    g_result->active = 1;

    // ----------------------------------------------- запись предыдущей команды
    const int command  = cmd->command_number;
    const int previous = command - 1;

    if (previous < 0)
        return;

    CmdRecord* record = GetRecord(previous);
    if (!record)
        return;

    bool replayed;
    if (record->predId == *pred && record->count > 1)
    {
        replayed = true;
    }
    else
    {
        if (record->predId != *pred)
            return;
        replayed = false;
    }

    // запись уже обработана
    if (record->flags & 0x20)
        return;

    FillCmdRecord(g_local, record->data, 17, record);

    // --------------------------------------------------------------- тики
    {
        const int   ticks = g_local->tickCount + 1;
        const float time  = (float)ticks * 0.015625f;

        g_local->tick     = ticks;
        g_local->tickTime = time;
        *(int*)((char*)g_local + 1900)   = ticks;
        *(float*)((char*)g_local + 1904) = time;
    }

    // ------------------------------------------------------ дополнительные
    uint64_t extraTicks = 0;
    int      extraLow   = 0;

    {
        const void* extra = GetExtraTicks(pred);
        extraTicks = *(const uint64_t*)extra;
        extraLow   = *(const int*)((const char*)extra + 8);
    }

    // --------------------------------------------------------- позиция глаз
    const Vector eye = g_local->viewOffset + g_local->origin;

    const int tickBase = (pawn && off_m_nTickBase)
                             ? ReadInt((char*)pawn + off_m_nTickBase)
                             : 0;

    CmdRecord* current = GetRecord(command);

    g_result->tick = command;

    // ------------------------------------------------ свежая запись лагкомпа
    int lag = -1;

    if (g_local->flags & 1)
    {
        for (int c = command; c >= 0 && c > command - 16; --c)
        {
            CmdRecord* r = GetRecord(c);
            if (!r || (r->flags & 1))
                continue;

            lag = r->lagIndex;
            break;
        }
    }

    g_local->lagRecord = lag;

    // ------------------------------------------------------- копия tick base
    if (Readable(pawn) && g_result->tick - 1 > 0 && g_tickFn)
    {
        const int want    = g_result->tick - 1;
        void*     tickPtr = ((void* (*)())g_tickFn)();

        if (Readable(tickPtr) && g_tickFnCount)
        {
            void* entry = (char*)tickPtr + g_tickFnStride * (want % g_tickFnCount);

            if (Readable(entry) && *(uint64_t*)((char*)entry + 8) == (uint64_t)want)
            {
                g_rage->field7DC = 0;

                void* a = *(void**)((char*)entry + 8);
                if (Readable(a))
                {
                    g_rage->field7DC = *(int*)((char*)a + 84);

                    void* b = *(void**)((char*)a + 8);
                    if (Readable(b))
                    {
                        g_rage->ring0 = *(int*)((char*)b + 24);
                        g_rage->ring1 = *(int*)((char*)b + 28);
                        g_rage->ring2 = *(int*)((char*)b + 32);
                    }
                }
            }
        }
    }

    // ---------------------------------------------------------- контекст скана
    g_result->resolved = 1;

    g_scanCmd      = (uint64_t)cmd;
    g_scanAngles   = (uint64_t)&g_viewAngles;
    g_scanButtons  = (uint64_t)&buttons;
    g_scanCallback = (uint64_t)OnScanTick;

    // чистим записи текущего тика
    {
        int count = (int)((g_scanList3 - g_scanList2) >> 9);

        while (count)
        {
            const int index = --count;

            if (*(int*)(g_scanList2 + (index << 9)) != g_result->tick)
                continue;

            char* first = (char*)g_scanList2 + (index << 9);
            char* it    = first + 512;
            char* end   = (char*)g_scanList3;

            while (it != end)
            {
                CommitTarget(first, it, 0, 0);
                first += 512;
                it    += 512;
            }

            g_scanList3 -= 512;

            if (index < (int)(0xAAAAAAAAAAAAAAABull * ((g_scanList1 - g_scanList0) >> 2)))
            {
                memmove((void*)(g_scanList0 + 12 * index),
                        (void*)(g_scanList0 + 12 * index + 12),
                        (size_t)(g_scanList1 - (g_scanList0 + 12 * index + 12)));

                g_scanList1 -= 12;
            }
        }
    }

    // сброс при смене тика
    if (g_result->tick != g_lastScanTick || g_result->resolved)
    {
        g_lastScanTime = -1.f;

        for (char* i = g_listBegin; i != g_listEnd; i += 506 * 8)
        {
            for (char* j = i + 18 * 8; j != i + 482 * 8; j += 29 * 8)
                *(int*)(j + 16) = 0;

            char* end1 = *(char**)(i + 88);
            for (char* k = *(char**)(i + 80); k != end1; k += 232)
                *(int*)(k + 16) = 0;

            char* end2 = *(char**)(i + 120);
            for (char* k = *(char**)(i + 112); k != end2; k += 232)
                *(int*)(k + 16) = 0;
        }
    }

    // ------------------------------------------------------------ нет оружия
    if (!g_local->weaponData)
    {
        g_result->field0A = 0;
        g_result->field0C = 0;
        return;
    }

    UpdateShotTelemetry(&g_viewAngles);

    // Результат команды заполняется до снимка: снимок читает из него
    // байты +5, +6 и +8.
    UpdateHookDistance();
    FillCommandResult(&g_viewAngles);

    SnapshotSettings();

    // ---------------------------------------------------------- сбор целей
    {
        char query[64] = {};

        *(void**)&query[0x00]    = g_local;
        *(uint64_t*)&query[0x08] = *(uint64_t*)((char*)g_result + 52);
        *(int*)&query[0x10]      = *(int*)((char*)g_result + 60);
        *(int*)&query[0x14]      = *(int*)((char*)g_result + 40);
        *(uint64_t*)&query[0x18] = *(uint64_t*)((char*)g_result + 44);
        *(int*)&query[0x20]      = g_local->tick;
        *(int*)&query[0x24]      = *(int*)((char*)g_result + 20);
        *(int*)&query[0x28]      = g_settings->doubleTap;
        *(int*)&query[0x2C]      = g_settings->removeSpreadMode;

        CollectTargets(query);

        if (NoTargets())
            g_result->canShoot = 0;
    }

    // ------------------------------------------------------ передача в контекст
    g_rage->limiters     = g_settings->limiters;
    g_rage->limiterFlags = g_settings->limiterExtra;
    g_rage->doubleTap    = g_settings->doubleTap;
    g_rage->silent       = g_silent;

    ScanTargets(g_rage, g_local, cmd, g_local->pawn, g_local->weapon);

    if (!g_settings->limiters || g_scanBegin == g_scanEnd)
        goto skip_limiters;

    // -------------------------------------------------------------- лимитеры
    {
        const int tick = g_local->tick;

        const bool blocked =
            (g_cmdFlags & 1)
                ? (tick == g_local->tickB
                       ? g_local->fracB > 0.f
                       : tick < g_local->tickB)
                : false;

        const bool allowed =
            (g_cmdFlags & 0x800) == 0
            || (tick == g_local->tickB
                    ? g_local->fracB > 0.f
                    : tick < g_local->tickB);

        if (allowed && !blocked)
            ApplyLimiters(cmd);
    }

skip_limiters:

    // ------------------------------------------------------ запрос на просчёт
    {
        char scratch[64] = {};

        void* request = BuildSolveRequest(scratch, g_rage, &g_viewAngles,
                                          (void*)buttons, &g_shotSlot);
        QueueSolveResult(g_solveQueue, request);
    }

    // ----------------------------------------------- состояние перед просчётом
    if ((g_local->flags & 1) == 0)
    {
        char state[64] = {};
        *(int*)&state[0x18] = -1;
        *(int*)&state[0x20] = -1;

        void* viewEntity = *(void**)((char*)cmd + 64);
        if (viewEntity && IsValidAddr((uintptr_t)viewEntity - 65452))
            *(int*)&state[0x00] = *(int*)((char*)viewEntity + 84);

        if (Readable(cmd))
            CopyViewAngles(&state[0x08], (char*)cmd + 32);

        BuildPredictRequest(state, &g_aimAngles, (void*)g_aimPunch, g_solveFlags, 0);

        *(int*)&state[0x04] = 2;

        BuildPosList(&state[0x40],
                     (unsigned)((*(int64_t*)&state[0x28] - *(int64_t*)&state[0x08]) / 172));

        for (char* it = *(char**)&state[0x08]; it != *(char**)&state[0x28]; it += 172)
        {
            struct { uint64_t a; uint8_t b; } entry;
            entry.a = *(uint64_t*)(it + 24);
            entry.b = *(uint8_t*)(it + 32);
            AppendPos(&state[0x40], &entry);
        }

        if (g_scanCmd)
        {
            void* viewEntity2 = *(void**)(g_scanCmd + 64);
            if (viewEntity2 && IsValidAddr((uintptr_t)viewEntity2 - 65428))
                *(int*)&state[0x3C] = *(int*)((char*)viewEntity2 + 108);
        }

        QueueSolveResult(g_solveQueue, state);
    }

    // -------------------------------------------- очистка состояния при стопе
    if (g_settings->autoStop || g_settings->autoScope)
    {
        memset(g_shotState, 0, 0xA8);
        g_shotState[6] = -1.f;

        if (!g_forceStop && !g_skipStop)
        {
            g_shotState[0] = 1.f;
            g_shotState[1] = (float)g_result->tick;
            g_shotState[2] = (float)g_local->tick;

            const float vx = *(float*)((char*)g_local + 12);
            const float vy = *(float*)((char*)g_local + 16);
            const float sq = vx * vx + vy * vy;

            g_shotState[3] = (sq < 0.f) ? FastSqrt(sq) : sqrtf(sq);
            g_shotState[4] = *(float*)((char*)g_local + 104);
            *(uint8_t*)&g_shotState[5] = (g_local->flags & 1) == 0;
        }
    }

    StopPass2((SolveContext*)g_solveCtx);

    // ------------------------------------------------------------------ нож
    if (g_local->weaponId == 64)
    {
        KnifeBot(g_settings->removeSpreadMode, &g_viewAngles, &buttons);
        goto emit;
    }

    // ------------------------------------------------------ стоп и задержка
    if (g_settings->limiters)
    {
        const int lastTick = g_rage->lastTick;

        if (g_local->tickCount < lastTick)
        {
            g_cmdFlags &= 0xFFFFFFFFFFFFF7FEull;
            buttons    &= 0xFFFFFFFFFFFFF7FEull;

            RemoveShots(0x801);

            g_cmdFlags |= g_local->buttons & 0x801;
        }
        else if (g_local->tickCount == lastTick)
        {
            ShotPos shot{};
            shot.angle = g_rage->lastTime + 131072.f;
            shot.flags = (uint16_t)(g_local->buttons & 0x801);

            AddShot(shot);
        }
    }

    // ---------------------------------------------------------- тип оружия
    {
        void* weaponData = g_local->weaponData;

        const int type = WeaponInt(weaponData, off_m_WeaponType);
        const int id   = g_local->weaponId;

        const bool burstWeapon = (unsigned)(type - 1) < 6;
        const bool autoSniper  = (id == 31);

        if (!burstWeapon && !autoSniper)
            goto emit;

        if (!WeaponInt(weaponData, off_m_bHasBurstMode)
            && WeaponInt(weaponData, off_m_nZoomLevels) <= 0)
            goto emit;

        if (type != 0 && id != 64)
        {
            g_cmdFlags &= ~0x800ull;
            buttons    &= ~0x800ull;
        }
    }

emit:
    // -------------------------------------------------------------- патроны
    if (g_local->clip <= 0)
    {
        void* weapon     = g_local->weapon;
        void* weaponData = g_local->weaponData;

        int type = -1;
        if (weaponData)
            type = WeaponInt(weaponData, off_m_WeaponType);

        if (weapon)
        {
            const int reserve = WeaponInt(weaponData, off_m_iMaxClip1);

            if (g_local->clip < reserve
                && ((unsigned)(type - 1) < 6 || g_local->weaponId == 31)
                && off_m_pReserveAmmo
                && *(int*)((char*)weapon + off_m_pReserveAmmo) > 0)
            {
                g_cmdFlags = (g_cmdFlags & 0xFFFFFFFFFFFFD7FEull) | 0x2000;
                buttons   &= 0xFFFFFFFFFFFFF7FEull;
            }
        }
    }

    UpdateShotBookkeeping(g_rage);

    // ------------------------------------------------------------- выстрел
    if (g_scanCmd && g_scanAngles && g_scanButtons)
    {
        if (!g_forceStop)
        {
        stop_pass:
            StopPass((SolveContext*)g_solveCtx);
            QuickStop((SolveContext*)g_solveCtx);
            goto after_solve;
        }

        if (g_skipStop)
        {
            StopVariant((SolveContext*)g_solveCtx);
            goto stop_pass;
        }

        if (!RefineShot((SolveContext*)g_solveCtx))
        {
            BuildShotState((SolveContext*)g_solveCtx);

            if (PickBest((SolveContext*)g_solveCtx))
                goto after_solve;

            FallbackSolve((SolveContext*)g_solveCtx);
            goto stop_pass;
        }

        if (g_shotState[0] != 0.f)
            *((uint8_t*)&g_shotState[5] + 1) = 1;
    }

after_solve:
    if (g_settings->limiters)
    {
        cmd->buttons = buttons;
        return;
    }

    // ------------------------------------------ запись позиции выстрела
    if (g_result->canShoot
        && (g_cmdFlags & 1)
        && g_local->weaponId != 64)
    {
        if (!WeaponHasSeedSync(g_local->weaponData) && g_local->ammo > 0)
        {
            ShotPos shot{};
            shot.angle = 0.f;
            *(uint64_t*)((char*)&shot + 8) = 1;

            AddShot(shot);
        }
    }

    // ------------------------------------------- флаги в кольце выстрелов
    {
        static const uint64_t kFlags[2] = { 4, 2 };

        for (int i = 0; i < 2; ++i)
        {
            const uint64_t flag = kFlags[i];

            if ((uint64_t)((g_shotsEnd - g_shotsBegin) / 72) > 0x1F)
            {
                if (flag == 2)
                    goto done;

                continue;
            }

            if (((g_local->buttons ^ g_cmdFlags) & flag) == 0)
                continue;

            bool exists = false;
            for (ShotPos* it = g_shotsBegin; it != g_shotsEnd; ++it)
            {
                if (*(uint64_t*)((char*)it + 8) == flag)
                {
                    exists = true;
                    break;
                }
            }

            if (exists)
                continue;

            ShotPos shot{};
            shot.angle = 0.f;
            *(uint64_t*)((char*)&shot + 8) = flag;
            *(uint8_t*)((char*)&shot + 16) = (flag & g_cmdFlags) != 0;

            AddShot(shot);
        }
    }

done:
    cmd->buttons = buttons;
}

// ============================================================================
//  Обёртки над общими помощниками
// ============================================================================
int SchemaOffset(const char* className, const char* fieldName)
{
    // Резолв полей схемы — внешний по отношению к rage: имена классов и
    // полей обрабатывает движок. Без него смещение считается неизвестным,
    // и все обращения к полям аккуратно отключаются.
    (void)className;
    (void)fieldName;
    return 0;
}

void* ReadPtrGuarded(const void* p)
{
    if (!p || !Readable(p))
        return nullptr;

    void* value = ReadPtr(p);
    return IsValid(value) ? value : nullptr;
}

// ---------------------------------------------------------------------------
//  Схема для записей выстрела
// ---------------------------------------------------------------------------
static int  off_m_collisionAttribute;
static int  off_m_usSolidFlags;
static int  off_m_hOwnerEntity;
static bool g_shotOffsetsReady;

extern void* g_shotRecordVtable;

static void ResolveShotOffsets()
{
    if (g_shotOffsetsReady || !g_schemaSystem)
        return;

    g_shotOffsetsReady      = true;
    off_m_pCollision        = GetSchemaOffset("C_BaseEntity",       "m_pCollision");
    off_m_collisionAttribute = GetSchemaOffset("CCollisionProperty", "m_collisionAttribute");
    off_m_usSolidFlags      = GetSchemaOffset("CCollisionProperty", "m_usSolidFlags");
    off_m_hOwnerEntity      = GetSchemaOffset("C_BaseEntity",       "m_hOwnerEntity");
}

// Индекс владельца записи: сущность из m_hOwnerEntity, приведённая к
// каноническому виду (младшие 15 бит), либо -1, если владельца нет.
static int GetOwnerIndex(void* entity)
{
    if (!entity || !off_m_pCollision)
        return -1;

    void* collision = ReadPtr((char*)entity + off_m_pCollision);
    if (!collision)
        return -1;

    if (ReadByte((char*)collision + off_m_usSolidFlags) & 4)
        return -1;

    const uint32_t handle = (uint32_t)ReadInt((char*)entity + off_m_hOwnerEntity);
    if (handle == 0xFFFFFFFFu)
        return -1;

    void* owner = GetEntityFromHandle(handle);
    if (!owner)
        return -1;

    void* inner = ReadPtr((char*)owner + 16);
    const int index = ReadInt((char*)inner + 16);
    const int flags = (uint16_t)(ReadWord((char*)inner + 48) << 15);

    return (index - flags) ^ (((uint16_t)index ^ (uint16_t)(index - flags)) & 0x7FFF);
}

// Готовит запись выстрела: обнуляет поля, ставит таблицу методов, владельца
// и снимает данные оружия из контекста решения.
void CopyShotRecord(void* dst, const void* src)
{
    if (!dst)
        return;

    ResolveShotOffsets();

    char*       record = (char*)dst;
    const char* ctx    = (const char*)src;

    memset(record, 0, 96);

    *(void**)record = g_shotRecordVtable;

    // Вектор по умолчанию — «нет направления».
    *(float*)(record + 32) = -1.f;
    *(float*)(record + 36) = -1.f;
    *(float*)(record + 40) = -1.f;
    *(float*)(record + 44) = -1.f;

    *(uint32_t*)(record + 50) = 0xFFFF0000u;
    *(uint16_t*)(record + 54) = 0x0F00u;
    *(uint8_t*)(record + 56)  = 11;
    *(uint8_t*)(record + 64)  = 0;

    *(uint64_t*)(record + 8) = *(uint64_t*)(ctx + 152);

    uint16_t solidFlags = 0;

    void* ownerEntity = ReadPtr(ctx + 328);
    if (ownerEntity)
    {
        void* inner = ReadPtr((char*)ownerEntity + 16);

        const int index = ReadInt((char*)inner + 16);
        const int shift = (uint16_t)(ReadWord((char*)inner + 48) << 15);

        *(uint32_t*)(record + 32) =
            (index - shift) ^ (((uint16_t)index ^ (uint16_t)(index - shift)) & 0x7FFF);

        if (off_m_pCollision)
        {
            void* collision = ReadPtr((char*)ownerEntity + off_m_pCollision);

            if (collision)
                solidFlags = ReadWord((char*)collision + off_m_collisionAttribute + 40);
        }
    }

    *(uint16_t*)(record + 48) = solidFlags;
    *(uint32_t*)(record + 40) = GetOwnerIndex(ownerEntity);
    *(uint64_t*)(record + 24) = 0x40000;
    *(uint8_t*)(record + 57)  = 75;
    *(uint64_t*)(record + 16) = 0;
}

// Добавляет точку прицеливания в список с учётом её геометрии и веса.
void AddAimPoint(void* ctx, void* list, void* point, void* record, void* extra,
                 float angleMax, float radius, void* target, const float* offset)
{
    char* l = (char*)list;
    if (!l)
        return;

    char entry[96] = {};
    CopyShotRecord(entry, record);

    *(float*)(entry + 0)  = *(float*)((char*)point + 0) + offset[0];
    *(float*)(entry + 4)  = *(float*)((char*)point + 4) + offset[1];
    *(float*)(entry + 8)  = *(float*)((char*)point + 8) + offset[2];
    *(float*)(entry + 12) = angleMax;
    *(float*)(entry + 16) = radius;
    *(void**)(entry + 24) = target;
    *(void**)(entry + 32) = extra;

    char* begin = *(char**)l;
    char* end   = *(char**)(l + 8);
    char* cap   = *(char**)(l + 16);

    if (end != cap)
    {
        memcpy(end, entry, 96);
        *(char**)(l + 8) = end + 96;
        return;
    }

    ShotPos* shot = (ShotPos*)entry;
    GrowShotList((ShotPos**)l, (ShotPos*)end, shot);
}

// Завершает резервный список и сразу выбирает из него лучшую запись.
void FinalizeFallback(void* ctx, void* list, void* target, int flag)
{
    char* l = (char*)list;

    *(void**)l = target;
    *(int*)(l + 24) = *(int*)((char*)ctx + 2820);
    *(uint64_t*)(l + 16) = *(uint64_t*)((char*)ctx + 4432);

    if (flag)
        *(uint8_t*)(l + 40) = 1;

    RegisterAimRecord(ctx, list, *(uint64_t*)((char*)ctx + 2800), 1);
    PickAimEntry(list, (char*)ctx + 7888);
}

// Проверяет попадание и, если оно есть, сохраняет точку в записи.
bool ApplyAimHit(void* base, void* entry, void* point, void* candidate)
{
    TestAimHit(base, entry, point, candidate);
    StoreAimHit(entry, candidate, point);

    return *(uint8_t*)((char*)entry + 12) != 0;
}

// Снимок параметров стоп-пути из контекста решения.
StopParams MakeStopParams(void* ctx)
{
    char* c = (char*)ctx;
    StopParams params{};

    params.ctx      = (SolveContext*)ctx;
    params.flag82   = *(uint8_t*)(c + 82);
    params.flag81   = *(uint8_t*)(c + 81);
    params.flag84   = *(uint8_t*)(c + 84);
    params.flag118  = *(uint8_t*)(c + 118);
    params.flag119  = *(uint8_t*)(c + 119);
    params.flag208  = *(uint8_t*)(c + 208);
    params.mode     = *(int*)(c + 96);
    params.callback = (void* (*)())OnScanTick;

    if (*(void**)(c + 8632))
        params.callback = *(void* (**)())(c + 8632);

    if (*(uint8_t*)(c + 8496))
    {
        params.extra0 = c + 8368;
        params.extra1 = c + 224;
    }

    return params;
}


// Чтение байта по адресу с проверкой читаемости.
uint8_t ReadByte(const void* p)
{
    return Readable(p) ? *(const uint8_t*)p : 0;
}


// ============================================================================
//  добавлено: part1.cpp
// ============================================================================
// ============================================================================
//  точки прицеливания: сбор, оценка и выбор лучшей
// ============================================================================

namespace
{

// 96-байтовая запись точки, которую CollectAimPoints кладёт в список.
// Поля заполняются по мере прохода: сначала позиция и тик, затем слот
// предсказания и признаки, а направление на точку дописывает вызывающий
// после GetAimPointData.
struct AimPointRecord
{
    Vector   pos;           // 0x00 позиция точки
    int      tick;          // 0x0C тик, к которому относится точка
    float    radius;        // 0x10 радиус точки
    uint8_t  valid;         // 0x14 признак «точка пригодна»
    char     pad0[3];
    int      field18;       // 0x18
    float    aimTime;       // 0x1C время прицеливания
    int      field20;       // 0x20
    int      field24;       // 0x24
    int      field28;       // 0x28
    uint8_t  flag2C;        // 0x2C
    char     pad1[3];
    ShotSlot slot;          // 0x30 слот предсказания
    uint8_t  hasSlot;       // 0x38
    char     pad2[3];
    int      field3C;       // 0x3C
    int      field40;       // 0x40
    uint8_t  flag44;        // 0x44
    char     pad3[3];
    int      field48;       // 0x48
    uint8_t  flag4C;        // 0x4C
    char     pad4[3];
    Vector   direction;     // 0x50 направление на точку
    int      field5C;       // 0x5C
};

static_assert(sizeof(AimPointRecord) == 96, "aim point record layout");
static_assert(offsetof(AimPointRecord, slot) == 48, "aim point slot offset");

// Пара «тик + дробная часть» — основной способ адресации времени в rage.
struct TickPair
{
    int   tick;
    float frac;
};

// ---------------------------------------------------------------------------
//  время
// ---------------------------------------------------------------------------

// Текущее время сервера: целый тик и его дробная часть.

TickPair part1_Now()
{
    TickPair now = { 0, 0.f };

    if (!g_local)
        return now;

    float whole = 0.f;
    now.frac = GetTimeFrac(g_local->tickTime, &whole);
    now.tick = (int)whole;

    return now;
}

// Приводит дробную часть к [0, 1), попутно перенося целые в тик.

TickPair part1_Normalize(int tick, float frac)
{
    if (frac < 0.f || frac >= 1.f)
    {
        float whole = 0.f;
        const float part = GetTimeFrac(frac, &whole);

        tick += (int)whole;
        frac  = part;

        if (frac < 0.f)
        {
            frac += 1.f;

            if (frac < 1.f)
                --tick;
            else
                frac = 0.f;
        }
    }

    TickPair result = { tick, frac };
    return result;
}

// Сравнение двух отметок времени: true, если lhs раньше rhs.

bool part1_Earlier(const TickPair& lhs, const TickPair& rhs)
{
    if (lhs.tick != rhs.tick)
        return lhs.tick < rhs.tick;

    return lhs.frac < rhs.frac;
}

// Разница rhs - lhs с нормализацией в [0, 1).

TickPair part1_Delta(const TickPair& rhs, const TickPair& lhs)
{
    int   tick = rhs.tick - lhs.tick;
    float frac = rhs.frac - lhs.frac;

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            --tick;
        else
            frac = 0.f;
    }

    TickPair result = { tick, frac };
    return result;
}

// ---------------------------------------------------------------------------
//  предсказание позиции
// ---------------------------------------------------------------------------

// История тиков: массив записей по 20 байт, кольцевой буфер на 32 записи.
const int kHistoryCount   = 1560;   // число записей
const int kHistoryHead    = 1564;   // индекс начала
const int kHistoryEntries = 920;    // первая запись
const int kHistoryStride  = 20;     // размер записи

// Позиция игрока на момент времени tick: линейная интерполяция между
// соседними записями истории. Если истории нет, берём текущую позицию
// и скорость.

Vector part1_Predict(char* record, const TickPair& tick)
{
    const int   count = *(int*)(record + kHistoryCount);
    const int   head  = *(int*)(record + kHistoryHead);
    const Vector origin = *(Vector*)(record);

    if (count < 2)
        return origin;

    // ищем две записи, между которыми лежит нужный тик
    for (int i = 0; i < count - 1; ++i)
    {
        const int a = ((head + i) % 32 + 32) % 32;
        const int b = ((head + i + 1) % 32 + 32) % 32;

        const int   tickA = *(int*)(record + kHistoryEntries + kHistoryStride * a);
        const float fracA = *(float*)(record + kHistoryEntries + kHistoryStride * a + 4);
        const int   tickB = *(int*)(record + kHistoryEntries + kHistoryStride * b);
        const float fracB = *(float*)(record + kHistoryEntries + kHistoryStride * b + 4);

        const TickPair pa = { tickA, fracA };
        const TickPair pb = { tickB, fracB };

        if (part1_Earlier(tick, pa))
            continue;

        if (!part1_Earlier(tick, pb))
            continue;

        const TickPair span = part1_Delta(pb, pa);
        const TickPair pos  = part1_Delta(tick, pa);

        const float whole = (float)span.tick + span.frac;
        if (whole <= 0.f)
            return origin;

        const float t = ((float)pos.tick + pos.frac) / whole;

        const Vector& posA = *(Vector*)(record + kHistoryEntries + kHistoryStride * a + 8);
        const Vector& posB = *(Vector*)(record + kHistoryEntries + kHistoryStride * b + 8);

        return posA + (posB - posA) * t;
    }

    // за пределами истории — экстраполяция по последней записи
    const int last = ((head + count - 1) % 32 + 32) % 32;

    const TickPair lastTick = {
        *(int*)(record + kHistoryEntries + kHistoryStride * last),
        *(float*)(record + kHistoryEntries + kHistoryStride * last + 4)
    };

    const Vector lastPos = *(Vector*)(record + kHistoryEntries + kHistoryStride * last + 8);
    const TickPair delta = part1_Delta(tick, lastTick);

    const float seconds = ((float)delta.tick + delta.frac) * (1.f / 64.f);

    const Vector velocity = *(Vector*)(record + 4);

    return lastPos + velocity * seconds;
}

// Проверяет, попадает ли отметка времени в историю; при попадании
// возвращает индекс записи.

bool part1_FindHistoryTick(char* record, const TickPair& tick, int* outIndex)
{
    const int count = *(int*)(record + kHistoryCount);
    if (count < 2)
        return false;

    const int head = *(int*)(record + kHistoryHead);

    for (int i = 0; i < count - 1; ++i)
    {
        const int index = ((head + i) % 32 + 32) % 32;

        const int   entryTick = *(int*)(record + kHistoryEntries + kHistoryStride * index);
        const float entryFrac = *(float*)(record + kHistoryEntries + kHistoryStride * index + 4);

        if (entryTick == tick.tick)
        {
            if (entryFrac <= tick.frac)
                continue;
        }
        else if (entryTick > tick.tick)
        {
            continue;
        }

        if (outIndex)
            *outIndex = head + i;

        return true;
    }

    return false;
}

// Ищет ближайший прошедший тик истории в окне [сейчас - 3, сейчас + 1].

bool part1_FindHistory(char* record, TickPair* out)
{
    const int count = *(int*)(record + kHistoryCount);
    if (count < 3)
        return false;

    const TickPair now = part1_Normalize(*(int*)(record + 348), *(float*)(record + 360));

    TickPair low  = part1_Normalize(now.tick - 3, now.frac);
    TickPair high = part1_Normalize(now.tick + 1, now.frac);

    const int head = *(int*)(record + kHistoryHead);

    for (int i = 0; i < count - 1; ++i)
    {
        const int index = ((head + i) % 32 + 32) % 32;

        TickPair entry = {
            *(int*)(record + kHistoryEntries + kHistoryStride * index),
            *(float*)(record + kHistoryEntries + kHistoryStride * index + 4)
        };

        if (part1_Earlier(entry, low) || part1_Earlier(high, entry))
            continue;

        *out = entry;
        return true;
    }

    return false;
}

// Базовая неточность оружия: первый уровень таблицы m_flSpread.

float part1_AimTimeBase(char* record)
{
    void* data = *(void**)(record + 1968);
    if (!data)
        return 0.f;

    static int offset = -1;
    if (offset < 0)
        offset = GetSchemaOffset("CCSWeaponBaseVData", "m_flSpread");

    if (!offset)
        return 0.f;

    return ReadFloat((char*)data + offset);
}

// Время прицеливания: базовая неточность оружия плюс поправка на зум.

float part1_AimTime(char* record)
{
    if (*(int*)(record + 24) == 2 || *(uint16_t*)(record + 1976) == 31)
        return 0.f;

    const int   zoom  = *(int*)(record + 1936);
    void*       data  = *(void**)(record + 1968);

    const int offSpread = GetSchemaOffset("CCSWeaponBaseVData", "m_flSpread");

    float extra = 0.f;
    if (data && offSpread)
    {
        const int level = (zoom > 1) ? 0 : zoom;
        extra = ReadFloat((char*)data + offSpread + 4 * level);
    }

    return part1_AimTimeBase(record) + extra;
}

// ---------------------------------------------------------------------------
//  вспомогательные проверки
// ---------------------------------------------------------------------------

// Отметка времени не должна опережать уже сохранённые окна.

bool part1_TickAllowed(char* record, int mode)
{
    const TickPair now = part1_Normalize(*(int*)(record + 348), *(float*)(record + 360));

    const int   tickA = *(int*)(record + (mode ? 860 : 852));
    const float fracA = *(float*)(record + (mode ? 864 : 856));

    const TickPair a = { tickA, fracA };

    if (part1_Earlier(a, now))
        return true;

    const int   tickB = *(int*)(record + (mode ? 852 : 860));
    const float fracB = *(float*)(record + (mode ? 856 : 864));

    const TickPair b = { tickB, fracB };
    return !part1_Earlier(now, b);
}

} // namespace

// ============================================================================
//  таблица весов
// ============================================================================

// Резервирует место под count записей по 520 байт. Память общая для всех
// проходов: список живёт в контексте решателя и только растёт.

void ReserveWeightTable(void* table, int count)
{
    const int stride = 520;

    char** list = (char**)table;
    const int have = (int)((list[1] - list[0]) / stride);

    if (count <= have)
    {
        if (count < have)
            list[1] = list[0] + (size_t)count * stride;

        return;
    }

    int capacity = (int)((list[2] - list[0]) / stride);

    if (count <= capacity)
    {
        list[1] = list[0] + (size_t)count * stride;
        return;
    }

    int grown = capacity + capacity / 2;
    if (grown < count)
        grown = count;

    char* memory = (char*)malloc((size_t)grown * stride);
    if (!memory)
        return;

    if (have > 0)
        memcpy(memory, list[0], (size_t)have * stride);

    if (list[0])
        free(list[0]);

    list[0] = memory;
    list[1] = memory + (size_t)count * stride;
    list[2] = memory + (size_t)grown * stride;
}

// ============================================================================
//  оценка точки
// ============================================================================

namespace
{

// Детерминированный генератор: в оригинале — вызовы игрового RandomFloat.

uint32_t part1_Random()
{
    static uint32_t state = 0x9E3779B9u;

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state;
}

float part1_RandomUnit()
{
    return (float)(part1_Random() & 0xFFFFFF) / 16777216.f;
}

// Обновляет таблицу разброса: 64 направления, которые используются при
// оценке шанса попадания. Таблица пересчитывается только при смене seed.

void part1_UpdateSpreadTable(char* base, float seed)
{
    const int offSeed  = 840;
    const int offCache = 844;
    const int offTable = 328;

    if (*(float*)(base + offSeed) == seed)
        return;

    *(float*)(base + offSeed)  = seed;
    *(float*)(base + offCache) = tanf(seed);

    float* table = (float*)(base + offTable);

    for (int i = 0; i < 64; ++i)
    {
        const float angle  = (float)i * (6.2831853f / 64.f);
        const float radius = (float)(i + 1) * (1.f / 64.f);

        table[2 * i]     = seed * cosf(angle) * radius;
        table[2 * i + 1] = seed * sinf(angle) * radius;
    }
}

// Считает шанс попадания по таблице разброса. Направление луча — из
// начала записи в точку; попаданием считается луч, проходящий не дальше
// радиуса точки.

void part1_EvaluateShot(void* base, void* entry)
{
    char* b = (char*)base;
    char* e = (char*)entry;

    const Vector* point = *(const Vector**)(e + 24);
    const char*   data  = *(const char**)(e + 16);

    if (!point)
    {
        *(float*)(e + 68) = 1.f;
        *(float*)(e + 72) = 0.f;
        *(float*)(e + 76) = 0.f;
        *(float*)(e + 80) = 0.f;
        return;
    }

    const Vector origin   = *(const Vector*)e;
    const Vector toPoint  = *point - origin;
    const float  distance = toPoint.Length();

    const float radius = *(const float*)((const char*)point + 28);
    const float damage = *(float*)(e + 12);

    float minDamage = 0.f;
    if (data)
        minDamage = *(const float*)(data + 104);

    const Vector forward = (distance > 0.001f) ? toPoint * (1.f / distance) : Vector(1.f, 0.f, 0.f);

    Vector right, up;
    if (fabsf(forward.z) < 0.99f)
    {
        right = Vector(forward.y, -forward.x, 0.f).Normalized();
        up    = Vector(-forward.x * forward.z, -forward.y * forward.z,
                       forward.x * forward.x + forward.y * forward.y).Normalized();
    }
    else
    {
        right = Vector(1.f, 0.f, 0.f);
        up    = Vector(0.f, 1.f, 0.f);
    }

    const float* table = (const float*)(b + 328);

    int   hits     = 0;
    int   safeHits = 0;
    float timeSum  = 0.f;
    float damageSum = 0.f;

    const float radiusSqr = radius * radius;

    for (int i = 0; i < 64; ++i)
    {
        Vector dir = forward + right * table[2 * i] + up * table[2 * i + 1];
        dir = dir.Normalized();

        const float along = toPoint.Dot(dir);
        if (along <= 0.f)
            continue;

        const float missSqr = toPoint.LengthSqr() - along * along;

        if (missSqr > radiusSqr)
            continue;

        ++hits;
        damageSum += damage;
        timeSum   += (float)i * (1.f / 64.f);

        if (damage >= minDamage)
            ++safeHits;
    }

    const float scale = 1.f / 64.f;

    *(float*)(e + 68) = (float)hits * scale;
    *(float*)(e + 72) = timeSum * scale;
    *(float*)(e + 76) = (float)safeHits * scale;
    *(float*)(e + 80) = damageSum * scale;
}

// Подтягивает углы записи к точке на долю fraction. Возвращает true,
// если после подтяжки луч всё ещё проходит через точку.

bool part1_RefineEntry(void* base, void* entry, float fraction)
{
    char* b = (char*)base;
    char* e = (char*)entry;

    Vector* point = *(Vector**)(e + 24);
    if (!point)
        return false;

    const Vector origin = *(const Vector*)e;
    const Vector want   = *point - origin;

    if (want.LengthSqr() < 1e-6f)
        return false;

    const float pitch = -asinf(want.z / want.Length()) * (180.f / 3.14159265f);
    const float yaw   = atan2f(want.y, want.x) * (180.f / 3.14159265f);

    Vector* angles = (Vector*)(e + 48);

    angles->x += (pitch - angles->x) * fraction;
    angles->y += (yaw   - angles->y) * fraction;
    angles->z  = 0.f;

    // грубая проверка: точка должна остаться впереди и в пределах радиуса
    const Vector dir = (*point - origin).Normalized();
    const Vector aim = Vector(cosf(angles->y * 0.017453292f) * cosf(angles->x * 0.017453292f),
                              sinf(angles->y * 0.017453292f) * cosf(angles->x * 0.017453292f),
                              -sinf(angles->x * 0.017453292f));

    const float along = want.Dot(aim);
    if (along <= 0.f)
        return false;

    const float missSqr = want.LengthSqr() - along * along;
    const float radius  = *(const float*)((const char*)point + 28);

    return missSqr <= radius * radius;
}

// Отмечает попадание в хитгруппу, чтобы бот не стрелял в одну и ту же
// группу повторно в рамках одной команды.

void part1_MarkHitgroup(char* base, char* entry)
{
    char* data = *(char**)(entry + 32);
    if (!data)
        return;

    char* owner = *(char**)(*(char**)(entry + 16) + 8);
    if (!owner)
        return;

    int group = *(uint8_t*)(data + 56);

    if (group == 1)
        return;

    if (group == 13)
        group = 16;
    else if (group == 14)
        group = 18;
    else if (group >= 19)
        return;

    const int command = *(int*)(base + 24);

    *(uint8_t*)(owner + 8 * group + 3892) = 1;

    if (*(int*)(owner + 8 * group + 3888) != command)
        *(int*)(owner + 8 * group + 3888) = command;
}

} // namespace

// ============================================================================
//  выбор и уточнение лучшей записи
// ============================================================================

// Выбирает лучшую запись из списка результатов, уточняет её углы и
// возвращает указатель на неё. Пустой результат — nullptr.

void* FinishAimList(void* base)
{
    char* b = (char*)base;

    *(void**)(b + 248) = nullptr;

    char* begin = *(char**)(b + 224);
    char* end   = *(char**)(b + 232);

    if (!begin || !end || end <= begin)
        return nullptr;

    const int count = (int)((end - begin) / 88);

    char* best = nullptr;

    if (count <= 511)
    {
        for (char* it = begin; it != end; it += 88)
        {
            if (*(float*)(it + 12) < 0.f)
                continue;

            if (!best || TryRefine(b, it, best, *(int*)(b + 200), 1))
                best = it;
        }
    }
    else
    {
        // На больших списках оригинал раскидывает перебор по потокам.
        // Результат тот же — лучшая запись по компаратору.
        for (char* it = begin; it != end; it += 88)
        {
            if (*(float*)(it + 12) < 0.f)
                continue;

            if (!best || TryRefine(b, it, best, *(int*)(b + 200), 1))
                best = it;
        }
    }

    *(void**)(b + 248) = best;

    if (!best)
        return nullptr;

    part1_EvaluateShot(b, best);

    // Если луч видит цель, уточнять нечего.
    if (RayTest(b, best))
    {
        part1_MarkHitgroup(b, best);
        return best;
    }

    Vector* point = *(Vector**)(best + 24);
    if (!point)
        return best;

    const Vector origin = *(const Vector*)best;
    const Vector toPoint = *point - origin;
    const float distance = toPoint.Length();

    // Число шагов уточнения зависит от разброса оружия и дистанции:
    // чем больше произведение, тем грубее сетка.
    char* pointData = *(char**)(best + 32);
    float spread = 0.f;

    if (pointData)
    {
        char* weapon = *(char**)(pointData + 32);
        if (weapon)
            spread = *(float*)(weapon + 516);
    }

    int steps = 3;

    const float span = spread * distance;
    if (span > 0.0001f)
    {
        const float raw = (24.f / span) * 3.f;
        steps = (int)(raw + 0.5f);

        if (steps < 2)
            steps = 2;
        if (steps > 3)
            steps = 3;
    }

    const float seed = part1_RandomUnit();
    part1_UpdateSpreadTable(b, seed);

    const float nextSeed = part1_RandomUnit();
    const float shrink   = 1.f - (seed > 0.f ? nextSeed / seed : 0.f);

    const Vector before = *(Vector*)(best + 48);

    for (int i = 0; i < steps; ++i)
    {
        const float denominator = (steps > 1) ? (float)(steps - 1) : 1.f;
        const float fraction = 1.f - ((float)i * shrink) / denominator;

        if (part1_RefineEntry(b, best, fraction))
            break;
    }

    const Vector after = *(Vector*)(best + 48);

    if (before.x != after.x || before.y != after.y || before.z != after.z)
    {
        // Углы изменились — пересчитываем шанс и при улучшении оставляем
        // уточнённый вариант.
        part1_EvaluateShot(b, best);

        if (*(float*)(best + 68) < *(float*)(b + 140))
            return best;
    }

    part1_MarkHitgroup(b, best);

    return best;
}

// ============================================================================
//  проверка записи перед выстрелом
// ============================================================================

// Трассирует луч из начала записи в точку и заполняет урон/слот/буфер
// попадания. В оригинале здесь работает движковый TraceRay.

void SelectAimEntry(void* base, void* entry)
{
    char* b = (char*)base;
    char* e = (char*)entry;

    if (!b || !e)
        return;

    char*   data  = *(char**)(e + 16);
    char*   hit   = *(char**)(e + 40);
    Vector* point = *(Vector**)(e + 24);

    if (!data || !point)
        return;

    const Vector origin = *(const Vector*)e;
    const Vector toPoint = *point - origin;
    const float  distance = toPoint.Length();

    if (distance < 0.001f)
        return;

    const float radius = *(const float*)((const char*)point + 28);
    const float damage = *(const float*)(data + 108);

    // Ближайшая точка луча к цели: если она дальше радиуса, луч прошёл мимо.
    const Vector dir = toPoint * (1.f / distance);
    const float along = distance;
    const float missSqr = toPoint.LengthSqr() - along * along;

    if (missSqr > radius * radius)
        return;

    const int tick = *(int*)((const char*)point + 12);

    *(int*)(e + 48) = tick;

    if (hit)
        *(void**)(e + 32) = hit;

    *(float*)(e + 12) = damage;

    if (*(float*)(data + 108) > damage)
        return;

    part1_EvaluateShot(b, e);

    if (*(float*)(b + 140) > *(float*)(e + 52))
        return;

    part1_MarkHitgroup(b, e);
}

// ============================================================================
//  направление на точку
// ============================================================================

namespace
{

// Предсказанная позиция цели по слоту: история хранится в подобъекте
// контекста, записи идут с шагом 12 байт.

Vector part1_PredictedPoint(char* ctx, const ShotSlot& slot)
{
    char* history = ctx + 224;

    const int   baseTick = *(int*)(history + 172);
    const float baseFrac = *(float*)(history + 176);

    const Vector origin   = *(Vector*)(history + 180);
    const Vector velocity = *(Vector*)(history + 192);

    const float dt = ((float)(slot.index - baseTick) + (slot.frac - baseFrac));

    return origin + velocity * (dt * (1.f / 64.f));
}

// Разброс оружия в градусах.

float part1_WeaponSpread(char* ctx)
{
    void* data = *(void**)(ctx + 2192);
    if (!data)
        return 0.f;

    const int offset = GetSchemaOffset("CCSWeaponBaseVData", "m_flSpread");
    if (!offset)
        return 0.f;

    return ReadFloat((char*)data + offset);
}

// Отклоняет направление на случайный угол внутри конуса разброса.

void part1_ApplyCone(Vector& dir, float spread, uint32_t& rng)
{
    if (spread <= 0.f)
        return;

    const float angle = ((float)(rng & 0xFFFF) / 65536.f) * 6.2831853f;
    const float radius = ((float)((rng >> 16) & 0xFFFF) / 65536.f) * spread * 0.017453292f;

    Vector right, up;
    if (fabsf(dir.z) < 0.99f)
    {
        right = Vector(dir.y, -dir.x, 0.f).Normalized();
        up    = right.Cross(dir);
    }
    else
    {
        right = Vector(1.f, 0.f, 0.f);
        up    = Vector(0.f, 1.f, 0.f);
    }

    dir = (dir + right * (cosf(angle) * radius) + up * (sinf(angle) * radius)).Normalized();
}

} // namespace

// Считает направление, в котором нужно стрелять, чтобы попасть в точку.

void* GetAimPointData(void* out, void* ctx, void* entry)
{
    Vector* result = (Vector*)out;
    *result = Vector();

    char* c = (char*)ctx;
    char* e = (char*)entry;

    if (!c || !e)
        return out;

    if (!*(void**)(c + 552) || !*(void**)(c + 2184) || !*(void**)(c + 2192))
        return out;

    ShotSlot slot = { 0, 0.f };

    if (*(uint8_t*)(e + 56))
        slot = *(ShotSlot*)(e + 48);
    else
        slot = ShotSlotIndex(*(int*)(c + 348));

    const Vector point = part1_PredictedPoint(c, slot);
    const Vector eye   = *(Vector*)(c + 140);

    Vector target = point;
    target.z += *(float*)(c + 148);

    Vector dir = target - eye;
    if (dir.LengthSqr() < 1e-6f)
        return out;

    dir = dir.Normalized();

    if (*(int*)(c + 96) == 0)
    {
        // Режим без баллистики: направление из готового снимка углов плюс
        // небольшой случайный разброс.
        Vector view = *(Vector*)(c + 2104);

        if (view.LengthSqr() > 1e-6f)
            dir = view.Normalized();
    }
    else
    {
        uint32_t rng = part1_Random();
        part1_ApplyCone(dir, part1_WeaponSpread(c), rng);
    }

    *result = dir;
    return out;
}

// ============================================================================
//  сбор точек
// ============================================================================

namespace
{

// Добавляет точку в выходной список.

void part1_AppendPoint(void* out, const AimPointRecord& point)
{
    AppendEntry(out, (void*)&point);
}

// Точка из уже готового списка предсказаний: используется, когда сбор
// запускают повторно на том же тике.

void part1_AppendLastPoint(char* record, AimQuery* query, void* out, int mode)
{
    if (!query->list)
        return;

    char** list = (char**)query->list;
    if (list[0] == list[1])
        return;

    const int savedTick   = *(int*)(record + 348);
    const int savedTime   = *(int*)(record + 360);
    const int savedTickB  = *(int*)(record + 1900);
    const int savedTimeB  = *(int*)(record + 1908);
    const int savedTimeC  = *(int*)(record + 1928);

    *(int*)(record + 348)  = query->tickNow - 1;
    *(int*)(record + 1900) = query->tickNow - 1;
    *(int*)(record + 1928) = *(int*)(record + 1872);
    *(int*)(record + 360)  = *(int*)(&query->frac);
    *(int*)(record + 1908) = *(int*)(&query->frac);

    if (part1_TickAllowed(record, mode))
    {
        const int count = (int)((list[1] - list[0]) / 172);
        int index = query->index;

        if (index < 0 || index >= count)
            index = count - 1;

        char* snapshot = list[0] + 172 * index;

        TickPair tick = part1_Normalize(*(int*)(record + 348), *(float*)(record + 360));

        if (*(int*)(record + 288) == query->tickNow && *(uint8_t*)(snapshot + 32))
            tick = *(TickPair*)(snapshot + 24);

        const Vector pos = part1_Predict(record, tick);

        AimPointRecord point = {};
        point.pos      = pos;
        point.tick     = *(int*)(record + 348);
        point.radius   = *(float*)(snapshot + 16);
        point.valid    = *(uint8_t*)(snapshot + 20);
        point.aimTime  = part1_AimTime(record);
        point.slot     = ShotSlot{ tick.tick, tick.frac };
        point.hasSlot  = 1;
        point.field3C  = *(int*)(snapshot + 12);
        point.field40  = *(int*)(snapshot + 16);
        point.flag44   = *(uint8_t*)(snapshot + 20);
        point.field48  = *(int*)(&query->frac);
        point.flag4C   = 1;

        part1_AppendPoint(out, point);
    }

    *(int*)(record + 348)  = savedTick;
    *(int*)(record + 360)  = savedTime;
    *(int*)(record + 1900) = savedTickB;
    *(int*)(record + 1908) = savedTimeB;
    *(int*)(record + 1928) = savedTimeC;
}

// Основной проход: определяет тик выстрела, ищет точку в истории и
// добавляет её в список.

void part1_CollectMain(char* record, AimQuery* query, void* out, int mode, bool force)
{
    const TickPair now = part1_Normalize(*(int*)(record + 348), *(float*)(record + 360));

    TickPair history = now;
    const bool hasHistory = part1_FindHistory(record, &history);

    const int   baseTick = *(int*)(record + (mode ? 860 : 852));
    const float baseFrac = *(float*)(record + (mode ? 864 : 856));

    TickPair shot = part1_Normalize(baseTick, baseFrac);

    // К базовому времени добавляем дробную часть текущего тика.
    shot = part1_Normalize(shot.tick + now.tick, shot.frac + now.frac);

    TickPair wanted = shot;
    if (hasHistory)
        wanted = part1_Delta(shot, history);

    int historyIndex = 0;
    const bool inHistory = part1_FindHistoryTick(record, wanted, &historyIndex);

    // Целимся в будущий тик, если он есть в истории.
    TickPair chosen = shot;
    bool     found  = inHistory;

    if (!found && query->flag1C && *(uint16_t*)(record + 1976) != 64)
    {
        void* aim = nullptr;

        if (FindAimPoint(record, mode != 0, &aim) && aim)
        {
            const TickPair candidate = *(TickPair*)aim;

            if (!part1_Earlier(chosen, candidate))
                chosen = candidate;

            found = true;
        }
    }

    // Дополнительно ищем тик с максимальным уроном в истории.
    if (query->flag1D && !query->flag1E && *(int*)(record + kHistoryCount) >= 3)
    {
        float bestDamage = -3.4028235e38f;
        int   bestIndex  = -1;
        TickPair bestTick = chosen;

        const int count = *(int*)(record + kHistoryCount);
        const int head  = *(int*)(record + kHistoryHead);

        for (int i = 0; i < count; ++i)
        {
            const int index = ((head + i) % 32 + 32) % 32;

            const int   tick = *(int*)(record + kHistoryEntries + kHistoryStride * index);
            const float frac = *(float*)(record + kHistoryEntries + kHistoryStride * index + 4);
            const float dmg  = *(float*)(record + kHistoryEntries + kHistoryStride * index + 16);

            if (dmg <= bestDamage)
                continue;

            const TickPair entry = { tick, frac };

            if (part1_Earlier(entry, now))
                continue;

            bestDamage = dmg;
            bestIndex  = index;
            bestTick   = entry;
        }

        if (bestIndex >= 0)
        {
            chosen = bestTick;
            found  = true;
        }
    }

    if (!found && !force)
        return;

    const Vector pos = part1_Predict(record, chosen);

    AimPointRecord point = {};
    point.pos      = pos;
    point.tick     = *(int*)(record + 348);
    point.aimTime  = part1_AimTime(record);
    point.slot     = ShotSlot{ chosen.tick, chosen.frac };
    point.hasSlot  = 1;
    point.field48  = query->field20;
    point.flag4C   = 1;

    part1_AppendPoint(out, point);

    // Если точка заметно сместилась относительно текущей позиции,
    // добавляем вторую — на следующий тик.
    const Vector origin = *(Vector*)(record);
    if ((pos - origin).LengthSqr() > 1.f)
    {
        const TickPair next = part1_Normalize(*(int*)(record + 348), *(float*)(record + 360));
        const Vector future = part1_Predict(record, next);

        AimPointRecord extra = {};
        extra.pos      = future;
        extra.tick     = next.tick;
        extra.aimTime  = point.aimTime;
        extra.slot     = ShotSlot{ next.tick, next.frac };
        extra.hasSlot  = 1;
        extra.flag44   = 1;
        extra.field48  = query->field20;
        extra.flag4C   = 1;

        part1_AppendPoint(out, extra);
    }
}

} // namespace

// Собирает точки, в которые имеет смысл стрелять в этом тике.

void CollectAimPoints(void* queryPtr, void* recordPtr, void* a3, void* a4, void* out)
{
    AimQuery* query = (AimQuery*)queryPtr;
    char*     record = (char*)recordPtr;

    if (!query || !record || !out)
        return;

    const int  mode  = (int)(uintptr_t)a4;
    const bool force = a3 != nullptr;

    // Повторный вызов на том же тике: список предсказаний уже готов,
    // достаточно взять из него последнюю запись.
    if (!query->flag0 && query->tickNow == *(int*)(record + 348))
    {
        part1_AppendLastPoint(record, query, out, mode);
        return;
    }

    part1_CollectMain(record, query, out, mode, force);
}


// ============================================================================
//  добавлено: part2.cpp
// ============================================================================
#include <cstdarg>
#include <cstdio>

// ============================================================================
//  разбор хендлов и записей команд
// ============================================================================

// Сущность по хендлу: биты 6..11 выбирают чанк, младшие 9 бит — слот в нём.

void* GetEntityFromHandle(int handle)
{
    const int chunkShift   = 6;
    const int chunkMask    = 0x1F8;     // ((handle >> 6) & 0x3F) * sizeof(void*)
    const int slotMask     = 0x1FF;
    const int serialMask   = 0x7FFF;
    const int chunkBase    = 16;        // начало массива чанков в системе сущностей
    const int identitySize = 112;       // sizeof(CEntityIdentity)
    const int serialOffset = 16;        // m_serialNumber

    void* system = ReadPtrGuarded(&g_worldEntity);
    if (!system)
        return nullptr;

    void* chunk = ReadPtrGuarded((char*)system + chunkBase + ((handle >> chunkShift) & chunkMask));
    if (!chunk)
        return nullptr;

    char* identity = (char*)chunk + identitySize * (handle & slotMask);
    if (!IsValid(identity + serialOffset))
        return nullptr;

    // серийник защищает от переиспользованного слота
    if ((ReadInt(identity + serialOffset) & serialMask) != (handle & serialMask))
        return nullptr;

    return ReadPtrGuarded(identity);
}

// Сколько подряд идущих команд помечены как «активные» перед указанной.

int GetHitCount(int slot)
{
    const int maxRecords = 150;         // размер кольца записей команд

    int command = slot - 1;
    if (command < 0)
        return -1;

    int count = 0;

    while (count < maxRecords - 1)
    {
        CmdRecord* record = GetRecord(command);
        if (!record)
            return -1;

        if (!record->count)
            return count;

        ++count;

        if (--command < 0)
            break;
    }

    return -1;
}

// Владелец оружия: хендл из сущности оружия, затем резолв в сущность.

static void* part2_WeaponOwner(void* weapon)
{
    static int  offset = 0;
    static bool ready  = false;

    if (!ready)
    {
        offset = SchemaOffset("C_BaseEntity", "m_hOwnerEntity");
        ready  = true;
    }

    if (!weapon || !offset)
        return nullptr;

    const int handle = ReadInt((char*)weapon + offset);
    if (!handle || handle == -1)
        return nullptr;

    return GetEntityFromHandle(handle);
}

// Запись команды для прохода: база, счётчик попаданий и владелец оружия.

void FillCmdRecord(void* dst, void* src, int count, void* extra)
{
    const int offOwner = 328;           // оружие, с которого считаем
    const int offEntity = 336;          // резолв владельца оружия
    const int offCount = 344;           // сколько попаданий учитывать
    const int offTick = 1872;           // тик, к которому привязана запись

    char* record = (char*)dst;
    char* source = (char*)src;

    if (!record || !source)
        return;

    InitStopPath(dst, src);

    void* owner = *(void**)&record[offOwner];
    *(int*)&record[offCount] = count;

    if (owner)
        *(void**)&record[offEntity] = part2_WeaponOwner(owner);

    // пересобираем рабочую область записи под новое число попаданий
    ResetStopPath();

    *(int*)&record[offTick] = *(int*)&source[offTick];
}

// ============================================================================
//  перенос записи цели
// ============================================================================

// Освобождает блок, которым владеет запись (begin/end/cap).

static void part2_FreeBlock(char* owner)
{
    const int    beginOff = 184;
    const int    endOff   = 192;
    const int    capOff   = 200;
    const size_t pageSize = 0x1000;     // выше этого размера выделение выровнено

    void* block = *(void**)(owner + beginOff);
    if (!block)
        return;

    const size_t capacity = 8 * ((size_t)(*(char**)(owner + capOff) - (char*)block) >> 3);

    // у больших выровненных блоков заголовок лежит перед данными
    void* allocated = (capacity >= pageSize) ? *(void**)((char*)block - sizeof(void*)) : block;

    free(allocated);

    *(void**)(owner + beginOff) = nullptr;
    *(void**)(owner + endOff)   = nullptr;
    *(void**)(owner + capOff)   = nullptr;
}

// Забирает владение блоком у источника.

static void part2_StealBlock(char* dst, char* src)
{
    const int beginOff = 184;
    const int endOff   = 192;
    const int capOff   = 200;

    *(void**)(dst + beginOff) = *(void**)(src + beginOff);
    *(void**)(dst + endOff)   = *(void**)(src + endOff);
    *(void**)(dst + capOff)   = *(void**)(src + capOff);

    *(void**)(src + beginOff) = nullptr;
    *(void**)(src + endOff)   = nullptr;
    *(void**)(src + capOff)   = nullptr;
}

// Переносит вложенный список: три указателя переезжают, источник обнуляется.

static void part2_MoveList(char* dst, char* src)
{
    const int beginOff = 0;
    const int endOff   = 8;
    const int capOff   = 16;

    *(void**)(dst + beginOff) = *(void**)(src + beginOff);
    *(void**)(dst + endOff)   = *(void**)(src + endOff);
    *(void**)(dst + capOff)   = *(void**)(src + capOff);

    *(void**)(src + beginOff) = nullptr;
    *(void**)(src + endOff)   = nullptr;
    *(void**)(src + capOff)   = nullptr;
}

// Полное присваивание записи цели: плоская часть, владеемый блок, вложенные
// списки и хвост. Семантика — move, как у сгенерированного оператора.

void CommitTarget(void* a, void* b, int c, int d)
{
    const int headerSize = 184;         // плоские поля записи до владеемого блока
    const int listAOff   = 208;         // первый вложенный список
    const int pairAOff   = 232;         // пара значений первого списка
    const int pairBOff   = 248;         // пара значений второго списка
    const int listBOff   = 264;         // второй вложенный список
    const int tailOff    = 288;         // плоский хвост записи
    const int tailSize   = 220;

    (void)c;
    (void)d;

    char* dst = (char*)a;
    char* src = (char*)b;

    if (!dst || !src)
        return;

    memcpy(dst, src, headerSize);

    if (dst != src)
    {
        part2_FreeBlock(dst);
        part2_StealBlock(dst, src);
    }

    part2_MoveList(dst + listAOff, src + listAOff);

    *(uint64_t*)(dst + pairAOff)     = *(uint64_t*)(src + pairAOff);
    *(uint64_t*)(dst + pairAOff + 8) = *(uint64_t*)(src + pairAOff + 8);
    *(uint64_t*)(dst + pairBOff)     = *(uint64_t*)(src + pairBOff);
    *(uint64_t*)(dst + pairBOff + 8) = *(uint64_t*)(src + pairBOff + 8);

    part2_MoveList(dst + listBOff, src + listBOff);

    memcpy(dst + tailOff, src + tailOff, tailSize);
}

// ============================================================================
//  проверка, что цель всё ещё достижима
// ============================================================================

// Может ли пуля из этой записи всё ещё дойти до цели: трассируем от оружия
// и смотрим, что луч прошёл всю дистанцию и не начался внутри геометрии.

bool CanStillShoot(void* obj)
{
    const int offWeapon     = 328;      // оружие, с которого стреляли
    const int offPawn       = 152;      // стрелок
    const int offFarFlag    = 512;      // 0 — дальняя проверка, иначе ближняя

    const int offFraction   = 0x9C;     // доля пройденного пути
    const int offStartSolid = 0xAB;     // старт внутри геометрии
    const int resultSize    = 192;

    const int offIdentity   = 16;       // CEntityIdentity в цепочке сущности
    const int offIndex      = 16;       // номер в цепочке
    const int offSerial     = 48;       // серийник
    const int offChecksum   = 0x0C;     // соль запроса в фильтре
    const int offFilterPawn = 0x08;
    const int offFilterMask = 0x10;

    const int     collisionMask = 0x40000;  // маска «пуля»
    const uint8_t contents      = 75;       // тип содержимого
    const int     serialMask    = 0x7FFF;
    const int     serialShift   = 15;
    const float   rangeNear     = 72.f;
    const float   rangeFar      = 55.f;
    const float   radius        = 16.f;
    const uint8_t traceType     = 2;

    char* state = (char*)obj;
    if (!state || !IsValidAddr((uintptr_t)state))
        return false;

    void* weapon = ReadPtrGuarded(state + offWeapon);
    void* pawn   = ReadPtrGuarded(state + offPawn);
    if (!weapon)
        return false;

    char filter[64] = {};
    char params[32] = {};
    char result[resultSize] = {};

    // хендл оружия сворачиваем в соль: индекс и серийник
    void* identity = ReadPtrGuarded((char*)weapon + offIdentity);
    if (identity)
    {
        const uint32_t index  = (uint16_t)ReadInt((char*)identity + offIndex);
        const uint32_t serial = (uint16_t)ReadInt((char*)identity + offSerial) << serialShift;
        const uint32_t delta  = index - serial;

        *(int*)(filter + offChecksum) = (int)(delta ^ ((index ^ delta) & serialMask));
    }

    *(void**)(filter + offFilterPawn) = pawn;
    *(uint32_t*)(filter + offFilterMask) = (uint32_t)collisionMask;
    *(uint8_t*)(filter + offFilterMask + 4) = contents;

    const bool nearRange = *(uint8_t*)(state + offFarFlag) == 0;

    *(float*)(params + 0x08)  = radius;
    *(float*)(params + 0x0C)  = nearRange ? rangeNear : rangeFar;
    *(uint8_t*)(params + 0x10) = traceType;

    if (!g_traceFnA)
        return false;

    TraceRay((void*)g_traceFnA, weapon, result);

    return *(uint8_t*)(result + offStartSolid) == 0
        && *(float*)(result + offFraction) >= 1.f;
}

// ============================================================================
//  разброс оружия
// ============================================================================

// Состояние генератора: одно на модуль, как и в бинаре.

static uint32_t& part2_RandState()
{
    static uint32_t state = 0x9E3779B9u;
    return state;
}

// Зерно выстрела: одинаковое зерно даёт одинаковый разброс.

static void part2_SeedRand(uint32_t seed)
{
    part2_RandState() = seed | 1u;
}

// Единичное случайное число из xorshift.

static float part2_RandUnit()
{
    uint32_t& state = part2_RandState();

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return (float)(uint16_t)state / 65535.f;
}

static float part2_Clamp01(float value)
{
    if (value < 0.f)
        return 0.f;

    return (value > 1.f) ? 1.f : value;
}

// Смещения CCSWeaponBaseVData, на которых держится весь расчёт разброса.
struct part2_SpreadOffsets
{
    int maxSpeed;
    int inaccuracyMove;
    int inaccuracyJumpInitial;
    int inaccuracyJumpApex;
    int inaccuracyLadder;
    int inaccuracyStand;
    int inaccuracyJump;
    int inaccuracyCrouch;
    int inaccuracyReload;
    int recoveryTimeCrouch;
    int recoveryTimeStand;
    int recoveryTimeStandFinal;
    int recoveryTimeCrouchFinal;
    int recoveryTransitionStart;
    int recoveryTransitionEnd;
    int cycleTime;
    int spread;
    int zoomLevels;
};

static part2_SpreadOffsets& part2_SpreadTable()
{
    static part2_SpreadOffsets offsets{};
    return offsets;
}

// Чтение float-поля оружия по разрешённому смещению и индексу режима.

static float part2_WeaponFloat(void* weaponData, int offset, int index)
{
    if (!weaponData || !offset || index < 0)
        return 0.f;

    const char* field = (const char*)weaponData + offset + (int)sizeof(float) * index;
    return IsValid(field) ? ReadFloat(field) : 0.f;
}

// Резолв схемы разброса: делается один раз за сессию.

void ComputeShotSpread()
{
    static bool ready = false;

    if (ready)
        return;

    if (!g_schemaSystem)
        return;

    ready = true;

    part2_SpreadOffsets& offsets = part2_SpreadTable();

    offsets.maxSpeed                = SchemaOffset("CCSWeaponBaseVData", "m_flMaxSpeed");
    offsets.inaccuracyMove          = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyMove");
    offsets.inaccuracyJumpInitial   = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyJumpInitial");
    offsets.inaccuracyJumpApex      = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyJumpApex");
    offsets.inaccuracyLadder        = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyLadder");
    offsets.inaccuracyStand         = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyStand");
    offsets.inaccuracyJump          = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyJump");
    offsets.inaccuracyCrouch        = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyCrouch");
    offsets.inaccuracyReload        = SchemaOffset("CCSWeaponBaseVData", "m_flInaccuracyReload");
    offsets.recoveryTimeCrouch      = SchemaOffset("CCSWeaponBaseVData", "m_flRecoveryTimeCrouch");
    offsets.recoveryTimeStand       = SchemaOffset("CCSWeaponBaseVData", "m_flRecoveryTimeStand");
    offsets.recoveryTimeStandFinal  = SchemaOffset("CCSWeaponBaseVData", "m_flRecoveryTimeStandFinal");
    offsets.recoveryTimeCrouchFinal = SchemaOffset("CCSWeaponBaseVData", "m_flRecoveryTimeCrouchFinal");
    offsets.recoveryTransitionStart = SchemaOffset("CCSWeaponBaseVData", "m_nRecoveryTransitionStartBullet");
    offsets.recoveryTransitionEnd   = SchemaOffset("CCSWeaponBaseVData", "m_nRecoveryTransitionEndBullet");
    offsets.cycleTime               = SchemaOffset("CCSWeaponBaseVData", "m_flCycleTime");
    offsets.spread                  = SchemaOffset("CCSWeaponBaseVData", "m_flSpread");
    offsets.zoomLevels              = SchemaOffset("CCSWeaponBaseVData", "m_nZoomLevels");

    // это же смещение читает внешний код через g_spreadStride
    g_spreadStride = offsets.spread;
}

// Время восстановления разброса: зависит от стойки и от фазы очереди.

static float part2_RecoveryTime(char* spread)
{
    const int offWeaponData = 88;
    const int offFlags      = 12;

    const part2_SpreadOffsets& offsets = part2_SpreadTable();
    void* weaponData = *(void**)(spread + offWeaponData);
    const uint32_t flags = *(uint32_t*)(spread + offFlags);

    int offset = offsets.recoveryTimeStandFinal;

    if (flags & 1)
        offset = (flags & 2) ? offsets.recoveryTimeCrouch : offsets.recoveryTimeStand;
    else
        offset = (flags & 2) ? offsets.recoveryTimeCrouchFinal : offsets.recoveryTimeStandFinal;

    const float value = part2_WeaponFloat(weaponData, offset, 0);
    return (value > 0.f) ? value : 1.f;
}

// Текущий разброс оружия: покой, движение, прыжок.

float GetShotSpread(void* obj)
{
    const int offSpeedSqr   = 4;
    const int offFlags      = 12;
    const int offCrouch     = 17;
    const int offAccum      = 48;
    const int offIndex      = 56;
    const int offMultiplier = 64;
    const int offCached     = 68;
    const int offFrozen     = 72;
    const int offWeaponData = 88;

    ComputeShotSpread();

    char* spread = (char*)obj;
    if (!spread)
        return 0.f;

    const float cached = *(float*)(spread + offCached);
    if (cached > 0.f)
        return cached;

    if (*(uint8_t*)(spread + offFrozen))
        return 0.f;

    void* weaponData = *(void**)(spread + offWeaponData);
    const part2_SpreadOffsets& offsets = part2_SpreadTable();

    int index = *(int*)(spread + offIndex);
    if (index > 1)
        index = 0;

    const float maxSpeed  = part2_WeaponFloat(weaponData, offsets.maxSpeed, index);
    const float moveInacc = part2_WeaponFloat(weaponData, offsets.inaccuracyMove, index);
    const float speed     = sqrtf(*(float*)(spread + offSpeedSqr));

    // доля скорости между «идёт» и «бежит»
    const float high = maxSpeed * 0.95f;
    const float low  = maxSpeed * 0.34f;

    float factor = 0.f;

    if (low == high)
    {
        if (speed >= high)
            factor = 1.f;
    }
    else
    {
        factor = (speed - low) / (high - low);
        factor = (factor >= 0.f) ? fminf(factor, 1.f) : 0.f;
    }

    float value = 0.f;

    if (factor > 0.f)
    {
        if (!*(uint8_t*)(spread + offCrouch))
            factor = part2_Clamp01(factor);

        value = moveInacc * factor;
    }

    value += *(float*)(spread + offAccum);

    if (*(uint32_t*)(spread + offFlags) & 1)
        return value;

    // в воздухе добавляется начальный разброс прыжка, дальше всё режет clamp
    const float multiplier = *(float*)(spread + offMultiplier);
    const float jump = part2_WeaponFloat(weaponData, offsets.inaccuracyJumpInitial, 0) * multiplier;

    return fminf(jump + value, 1.f);
}

// Накопление разброса за тик: подтягиваем текущее значение к базовому.

void UpdateShotSpread(void* obj)
{
    const int offFlags      = 12;
    const int offLadder     = 16;
    const int offCrouch     = 17;
    const int offNextAttack = 24;
    const int offRecoil     = 44;
    const int offAccum      = 48;
    const int offExtra      = 52;
    const int offIndex      = 56;
    const int offReloading  = 60;
    const int offMultiplier = 64;
    const int offWeaponData = 88;

    const float logBase      = 10.f;        // база логарифма затухания
    const float tickFraction = 0.015625f;   // 1/64 — доля тика
    const float minRecoil    = 0.1f;

    ComputeShotSpread();

    char* spread = (char*)obj;
    if (!spread)
        return;

    void* weaponData = *(void**)(spread + offWeaponData);
    const part2_SpreadOffsets& offsets = part2_SpreadTable();

    int index = *(int*)(spread + offIndex);
    if (index > 1)
        index = 0;

    const uint32_t flags = *(uint32_t*)(spread + offFlags);

    float value = 0.f;

    if (*(uint8_t*)(spread + offLadder) == 9)
    {
        // на лестнице работает только собственный разброс
        value = part2_WeaponFloat(weaponData, offsets.inaccuracyLadder, 0)
              + part2_WeaponFloat(weaponData, offsets.inaccuracyLadder, index);
    }
    else if (flags & 1)
    {
        const int offset = (flags & 2) ? offsets.inaccuracyCrouch : offsets.inaccuracyStand;
        value = part2_WeaponFloat(weaponData, offset, index);
    }
    else
    {
        const float jump = part2_WeaponFloat(weaponData, offsets.inaccuracyJump, index);
        value = jump * *(float*)(spread + offMultiplier)
              + part2_WeaponFloat(weaponData, offsets.inaccuracyStand, index);
    }

    if (*(uint8_t*)(spread + offReloading))
        value += part2_WeaponFloat(weaponData, offsets.inaccuracyReload, 0);

    if (value <= *(float*)(spread + offAccum))
    {
        // экспоненциальное приближение к текущему значению
        const float recovery = part2_RecoveryTime(spread);
        const float blend = expf((logf(logBase) / recovery) * -tickFraction);

        value = (*(float*)(spread + offAccum) - value) * blend + value;
    }

    *(float*)(spread + offAccum) = value;

    if (*(float*)(spread + offRecoil) > 0.f)
    {
        const float cycle = part2_WeaponFloat(weaponData, offsets.cycleTime, 0);

        if (*(float*)(spread + offNextAttack) > cycle + *(float*)(spread + offExtra) + tickFraction)
        {
            const float blend = expf(logf(logBase) * -2.f * tickFraction);
            const float next  = blend * *(float*)(spread + offRecoil);

            *(float*)(spread + offRecoil) = (next > minRecoil) ? next : 0.f;
        }
    }
}

// ============================================================================
//  геометрия разброса
// ============================================================================

// Базис обзора по углам (градусы).

static void part2_Basis(const Vector& angles, Vector& forward, Vector& right, Vector& up)
{
    const float degToRad = 0.017453292f;

    const float pitch = angles.x * degToRad;
    const float yaw   = angles.y * degToRad;

    const float sp = sinf(pitch);
    const float cp = cosf(pitch);
    const float sy = sinf(yaw);
    const float cy = cosf(yaw);

    forward = Vector(cp * cy, cp * sy, -sp);
    right   = Vector(-sy, cy, 0.f);
    up      = Vector(forward.y * right.z - forward.z * right.y,
                     forward.z * right.x - forward.x * right.z,
                     forward.x * right.y - forward.y * right.x);
}

// Направление разброса для текущего зерна: точка в конусе вокруг выстрела.

static void part2_SpreadBasis(float* out)
{
    const float fullTurn = 6.2831853f;

    const float angle  = part2_RandUnit() * fullTurn;
    const float radius = sqrtf(part2_RandUnit());

    out[0] = cosf(angle) * radius;
    out[1] = sinf(angle) * radius;
    out[2] = 0.f;
    out[3] = 0.f;
}

// Кривая отдачи: доля пути пули внутри тика для заданного конуса.

static float part2_RecoilCurve(float t, float cosine)
{
    const float cone     = 0.215638f;
    const float curveA   = 1.0904f;
    const float curveB   = 3.2451999f;
    const float curveC   = 3.5564499f;
    const float curveD   = 1.43519f;
    const float linear   = 1.06021f;
    const float baseline = 0.84801298f;

    const float base   = cone * cosine * cosine;
    const float linear2 = cosine * linear;
    const float poly   = (curveA - cosine * curveB + (cosine * curveC) * cosine)
                       - ((cosine * curveD) * cosine) * cosine;

    const float delta = t - 0.5f;

    return (t - 1.f) * t * delta * (poly * delta * delta + baseline - linear2 + base) + t;
}

// Уводит прицел в сторону разброса: точка, в которую реально уйдёт пуля.

void DropTarget(void* rage, void* target)
{
    const int offOrigin = 164;          // точка, из которой считаем направление
    const int offCone   = 44;           // конус разброса
    const int offAngles = 4464;         // выходные углы

    const float radToDeg = 57.295776f;

    char* ctx = (char*)rage;
    const float* point = (const float*)target;

    if (!ctx || !point)
        return;

    Vector* out = *(Vector**)(ctx + offAngles);
    if (!out)
        return;

    const Vector origin(*(float*)(ctx + offOrigin),
                        *(float*)(ctx + offOrigin + 4),
                        *(float*)(ctx + offOrigin + 8));

    const Vector delta = *(const Vector*)point - origin;
    const float  distance = delta.Length();

    if (distance <= 0.f)
        return;

    // чем ближе цель, тем сильнее сказывается конус
    const float factor = fminf(*(float*)(ctx + offCone) / distance, 1.f);

    const Vector* view = (const Vector*)GetViewAngles();
    if (!view)
        return;

    Vector forward, right, up;
    part2_Basis(*view, forward, right, up);

    float spreadDir[4];
    part2_SpreadBasis(spreadDir);

    const Vector spreadVector(spreadDir[0], spreadDir[1], spreadDir[2]);

    const float cosine = fabsf(forward.Dot(spreadVector));
    const float curve  = part2_RecoilCurve(factor, cosine);
    const float bend   = (forward.Dot(spreadVector) > 0.f) ? curve : -curve;

    const Vector aim = (forward * (1.f - curve) + spreadVector * bend).Normalized();

    const float yaw   = atan2f(aim.y, aim.x) * radToDeg;
    const float pitch = atan2f(-aim.z, sqrtf(aim.x * aim.x + aim.y * aim.y)) * radToDeg;

    out->x = pitch;
    out->y = yaw;
    out->z = 0.f;
}

// ============================================================================
//  скан целей: таблица разброса и выбор записи
// ============================================================================

// Запись таблицы разброса: углы пули и момент, к которому она относится.
struct part2_SpreadEntry
{
    float   pitch;                      // +0x00
    float   yaw;                        // +0x04
    float   spread;                     // +0x08
    float   scale;                      // +0x0C
    float   time;                       // +0x10
    int     pad0;                       // +0x14
    int     tick;                       // +0x18
    float   frac;                       // +0x1C
    uint8_t valid;                      // +0x20
    char    pad1[0x8B];
};

// Смещения состояния скана. Живёт между вызовами, как глобал в бинаре.
namespace part2_scan
{
    constexpr int active       = 0x001; // скан разрешён
    constexpr int counter      = 0x004; // счётчик проходов (больше 63 — пересборка)
    constexpr int tracking     = 0x008; // включён трекинг выстрела
    constexpr int baseSpread   = 0x7D8; // spread в начале окна
    constexpr int baseTick     = 0x7DC; // тик начала окна
    constexpr int spreadTable  = 0x7E0; // 16 значений на пули
    constexpr int lastShotTick = 0x820;
    constexpr int lastShotTime = 0x824;
    constexpr int command      = 0x828; // команда, для которой посчитано окно
    constexpr int cachedTick   = 0x82C;
    constexpr int cachedTime   = 0x830;
    constexpr int nextTick     = 0x834;
    constexpr int nextTime     = 0x838;
    constexpr int picked       = 0x83C; // выбранная запись
    constexpr int pickedTime   = 0x840;
    constexpr int dirty        = 0x84C; // таблицу надо пересобрать
}

// Окно выстрела: тик и доля времени, к которым привязан текущий выстрел.

static void part2_UpdateShotWindow(char* state)
{
    // предыдущее окно становится текущим
    *(int*)(state + part2_scan::cachedTick)   = *(int*)(state + part2_scan::nextTick);
    *(float*)(state + part2_scan::cachedTime) = *(float*)(state + part2_scan::nextTime);

    float carry = 0.f;
    float frac  = GetTimeFrac(g_local ? g_local->tickTime : 0.f, &carry);
    int   tick  = (int)carry;

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            tick -= 1;
        else
            frac = 0.f;
    }

    *(float*)(state + part2_scan::nextTime) = frac;
    *(int*)(state + part2_scan::nextTick)   = tick;

    // следующее окно не может откатиться назад относительно текущего
    const float cachedTime = *(float*)(state + part2_scan::cachedTime);
    const int   cachedTick = *(int*)(state + part2_scan::cachedTick);
    const int   nextTick   = *(int*)(state + part2_scan::nextTick);

    const bool advance = (cachedTick + 1 == nextTick)
                             ? *(float*)(state + part2_scan::nextTime) > cachedTime
                             : cachedTick + 1 < nextTick;

    if (advance)
    {
        *(float*)(state + part2_scan::nextTime) = cachedTime;
        *(int*)(state + part2_scan::nextTick)   = cachedTick + 1;
    }
}

// Последний выстрел из истории команд: тик и время.

static void part2_FindLastShot(int command, char* state)
{
    const int historyDepth = 150;
    const int offShotCount = 40;        // число выстрелов в записи команды
    const int offShotArray = 48;        // массив выстрелов
    const int offShotFlags = 16;        // флаги выстрела
    const int offShotTick  = 104;
    const int offShotTime  = 108;

    const int tickFlag = 0x800;
    const int timeFlag = 0x1000;

    for (int i = command - 1; i >= command - historyDepth; --i)
    {
        if (i <= 0)
            break;

        CmdRecord* record = GetRecord(i);
        if (!record)
            break;

        const int count = *(int*)(record->data + offShotCount);
        if (count <= 0)
            continue;

        void* array = ReadPtrGuarded(record->data + offShotArray);
        if (!array)
            continue;

        void* shot = ReadPtrGuarded((char*)array + (int)sizeof(void*) * count);
        if (!shot)
            continue;

        const int flags = ReadInt((char*)shot + offShotFlags);

        if (flags & tickFlag)
            *(int*)(state + part2_scan::lastShotTick) = ReadInt((char*)shot + offShotTick);

        if (flags & timeFlag)
            *(float*)(state + part2_scan::lastShotTime) = ReadFloat((char*)shot + offShotTime);

        break;
    }
}

// Пересобирает таблицу разброса на 16 пуль и заполняет записи под них.

static int part2_RebuildSpread(char* state, void* weapon, int command, int tick, int carry,
                               part2_SpreadEntry* entries, int capacity)
{
    const int   bullets    = 16;
    const float seedStride = 1.f;       // шаг зерна на команду
    const float radToDeg   = 57.295776f;
    const int   serialMask = 0x7FFF;
    const int   offIdentity = 16;       // цепочка сущности оружия
    const int   offIndex    = 16;       // номер в цепочке

    // зерно выстрела: номер сущности оружия и команда
    uint32_t seed = 0;

    void* identity = weapon ? ReadPtrGuarded((char*)weapon + offIdentity) : nullptr;
    if (identity)
        seed = (uint32_t)ReadInt((char*)identity + offIndex) & serialMask;

    seed += (uint32_t)((float)command * seedStride);

    part2_SeedRand(seed);

    const float baseSpread = *(float*)(state + part2_scan::baseSpread);
    const float delta      = GetCurrentSpread() - baseSpread;

    int count = 0;

    for (int bullet = 0; bullet < bullets && count < capacity; ++bullet)
    {
        // каждая следующая пуля идёт чуть дальше по кривой отдачи
        const float jitter = part2_RandUnit() * 0.05f - 0.025f;
        const float t = (bullet == bullets - 1) ? 1.f
                                                : (float)(bullet + 1) * 0.0625f + jitter;

        *(float*)(state + part2_scan::spreadTable + (int)sizeof(float) * bullet) = t;

        float base[4];
        float dir[4];

        part2_SpreadBasis(base);
        part2_SpreadBasis(dir);

        const float dot = base[0] * dir[0] + base[1] * dir[1]
                        + base[2] * dir[2] + base[3] * dir[3];

        const float curve = part2_RecoilCurve(t, fabsf(dot));
        const float bend  = (dot > 0.f) ? curve : -curve;

        float mixed[4];

        for (int i = 0; i < 4; ++i)
            mixed[i] = base[i] * (1.f - curve) + dir[i] * bend;

        const float length = sqrtf(mixed[0] * mixed[0] + mixed[1] * mixed[1]
                                 + mixed[2] * mixed[2] + mixed[3] * mixed[3]);

        if (length > 0.f)
        {
            for (int i = 0; i < 4; ++i)
                mixed[i] /= length;
        }

        const float horizontal = sqrtf(mixed[0] * mixed[0] + mixed[1] * mixed[1]);

        part2_SpreadEntry& entry = entries[count++];

        entry.pitch  = atan2f(-mixed[2], horizontal) * radToDeg;
        entry.yaw    = atan2f(mixed[1], mixed[0]) * radToDeg;
        entry.spread = t;
        entry.scale  = delta * t + baseSpread;
        entry.time   = *(float*)(state + part2_scan::lastShotTime);
        entry.tick   = tick + carry;
        entry.frac   = t;
        entry.valid  = 1;
    }

    return count;
}

// Скан целей: держит таблицу разброса и выбирает запись под текущую команду.

void ScanTargets(void* rage, void* local, void* cmd, void* pawn, void* weapon)
{
    (void)rage;
    (void)pawn;

    static char              state[0x860];   // состояние скана между вызовами
    static part2_SpreadEntry entries[64];
    static int               entryCount = 0;

    const int rebuildThreshold = 63;    // после стольких проходов таблица пересобирается
    const int shotCooldown     = 8;     // столько тиков выстрел считается свежим
    const int maxEntries       = (int)(sizeof(entries) / sizeof(entries[0]));
    const int bulletLimit      = 16;    // столько записей участвует в пересчёте
    const float timeBase       = 0.8f;
    const float timeRange      = 0.19f;

    const int command     = cmd ? ((CUserCmd*)cmd)->command_number : 0;
    const int currentTick = local ? ((LocalData*)local)->tick : 0;

    // счётчики текущего прохода
    *(int*)(state + part2_scan::lastShotTick)   = 0;
    *(float*)(state + part2_scan::lastShotTime) = 0.f;
    *(int*)(state + part2_scan::picked)         = -1;
    *(uint8_t*)(state + part2_scan::dirty)      = 0;

    if (!*(uint8_t*)(state + part2_scan::active))
    {
        // скан выключен: сбрасываем окно и оставляем только свежую запись
        *(int*)(state + part2_scan::command) = -1;

        if (entryCount >= 2)
        {
            entries[0] = entries[entryCount - 1];
            entryCount = 1;
        }

        *(uint8_t*)(state + part2_scan::dirty) = 1;

        if (*(uint8_t*)(state + part2_scan::tracking) && entryCount)
            entries[0].valid = 0;

        return;
    }

    if (*(int*)(state + part2_scan::command) != command)
    {
        *(int*)(state + part2_scan::command) = command;
        part2_UpdateShotWindow(state);
    }

    part2_FindLastShot(command, state);

    // давно не стреляли — окно сдвигается на текущий тик
    if (*(int*)(state + part2_scan::lastShotTick) <= currentTick - shotCooldown)
    {
        *(int*)(state + part2_scan::lastShotTick)   = currentTick - 1;
        *(float*)(state + part2_scan::lastShotTime) = part2_RandUnit() * timeRange + timeBase;
    }

    // доля времени внутри тика и длина окна в тиках
    float carry = 0.f;
    float frac  = GetTimeFrac(g_local ? g_local->tickTime : 0.f, &carry);

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            carry -= 1.f;
        else
            frac = 0.f;
    }

    float elapsed = frac - *(float*)(state + part2_scan::lastShotTime);
    int   span    = currentTick + (int)carry + 1 - *(int*)(state + part2_scan::lastShotTick);

    if (elapsed < 0.f)
    {
        elapsed += 1.f;

        if (elapsed >= 1.f)
            elapsed = 0.f;
        else
            --span;
    }

    if (*(int*)(state + part2_scan::counter) > rebuildThreshold)
    {
        entryCount = part2_RebuildSpread(state, weapon, command, currentTick, (int)carry,
                                         entries, maxEntries);
    }
    else
    {
        if (!entryCount)
            return;

        const float window = (float)span + elapsed;

        for (int i = 0; i < entryCount; ++i)
        {
            part2_SpreadEntry& entry = entries[i];

            if (!entry.valid)
                continue;

            // положение пули внутри окна пересчитываем только для первых пуль
            if (i < bulletLimit && window > 0.f)
            {
                float entryFrac = entry.time - *(float*)(state + part2_scan::lastShotTime);
                int   entryTick = entry.tick - *(int*)(state + part2_scan::lastShotTick);

                if (entryFrac < 0.f)
                {
                    entryFrac += 1.f;

                    if (entryFrac >= 1.f)
                        entryFrac = 0.f;
                    else
                        --entryTick;
                }

                *(float*)(state + part2_scan::spreadTable + (int)sizeof(float) * i) =
                    ((float)entryTick + entryFrac) / window;
            }

            if (*(uint8_t*)(state + part2_scan::tracking))
            {
                entry.valid = 0;
                *(uint8_t*)(state + part2_scan::dirty) = 1;
            }
        }
    }

    if (!entryCount)
        return;

    // выбор записи: в трекинге всегда последняя, иначе — по зерну
    int picked = entryCount - 1;

    if (!*(uint8_t*)(state + part2_scan::tracking))
    {
        if (*(int*)(state + part2_scan::counter) <= rebuildThreshold && entryCount > 1)
        {
            picked = (int)(part2_RandUnit() * (float)entryCount);

            if (picked >= entryCount)
                picked = entryCount - 1;
        }

        if (entries[picked].valid)
            *(float*)(state + part2_scan::pickedTime) = frac;
    }

    *(int*)(state + part2_scan::picked) = picked;
}

// ============================================================================
//  трассировка до цели
// ============================================================================

// Открыто ли окно выстрела для текущего тика.

static bool part2_WindowOpen(int tick, float frac, int windowTick, float windowTime)
{
    if (tick == windowTick)
        return frac < windowTime;

    return tick < windowTick;
}

// Точка выстрела: позиция, тик и разброс на этот выстрел (24 байта).
struct part2_Origin
{
    Vector origin;
    int    deltaTick;
    int    tick;
    float  spread;
};

static_assert(sizeof(part2_Origin) == 24, "origin entry");

// Переносит точку выстрела в список контекста (запись 96 байт).

static void part2_PushOrigin(void* list, const part2_Origin& origin)
{
    const int entrySize = 96;
    const int offTick   = 16;
    const int offDelta  = 24;
    const int offSpread = 28;

    char entry[entrySize] = {};

    *(Vector*)entry              = origin.origin;   // точка выстрела
    *(int*)(entry + 12)          = origin.tick;     // тик точки
    *(int*)(entry + offTick)     = origin.tick;
    *(int*)(entry + offDelta)    = origin.deltaTick;
    *(float*)(entry + offSpread) = origin.spread;

    PushShotOrigin(list, entry);
}

// Раскладка запроса прохода: два кэша углов, маски кнопок и список записей.
namespace part2_request
{
    constexpr int size    = 128;
    constexpr int maskA   = 0x08;
    constexpr int maskB   = 0x18;
    constexpr int anglesA = 0x2C;
    constexpr int anglesB = 0x70;
    constexpr int list    = 0x40;
    constexpr int listEnd = 0x48;
    constexpr int listCap = 0x50;
}

// Добавляет запись в список запроса. Список ограничен 32 записями, поэтому
// роста не требуется: при заполнении запись просто не добавляется.

static void part2_AppendShot(char* request, const char* entry, int entrySize)
{
    char* end = *(char**)(request + part2_request::listEnd);

    if (end == *(char**)(request + part2_request::listCap))
        return;

    memcpy(end, entry, (size_t)entrySize);
    *(char**)(request + part2_request::listEnd) = end + entrySize;
}

// Проход трассировки: собирает точки выстрела, проверяет окна и возвращает
// лучшую запись прицеливания.

void* TraceToTarget(void* ctx, unsigned flags, int tick, void* aim, void* target)
{
    // ---- смещения контекста решения
    const int offLocalData  = 2472;     // снимок локального состояния
    const int offStopList   = 8504;     // стоп-точки от лимитеров
    const int offMode       = 96;
    const int offWeaponId   = 4448;
    const int offKnifeCb    = 8632;
    const int offTick       = 2820;
    const int offRefTick    = 2760;
    const int offCycle      = 2832;
    const int offWinA       = 3340;
    const int offWinATime   = 3344;
    const int offWinB       = 3324;
    const int offWinBTime   = 3328;
    const int offSlot       = 188;
    const int offFlag81     = 81;
    const int offLimit      = 84;
    const int offFlag118    = 118;
    const int offFlag36     = 36;
    const int offFlag208    = 208;
    const int offFlag8      = 8;
    const int offPair       = 2800;
    const int offButtons    = 4432;
    const int offAnglesOut  = 4464;
    const int offState      = 7040;
    const int offOriginList = 7096;
    const int offTargetList = 7072;
    const int offAimData    = 7888;

    // ---- смещения записи трассы
    const int offRecOrigin     = 0x00;  // точка выстрела
    const int offRecOffset     = 0x24;  // смещение от неё
    const int offRecFlags      = 0x68;  // флаги команды
    const int offRecOriginTick = 0x120; // тик, к которому привязана точка
    const int offRecTick       = 0x15C; // текущий тик записи
    const int offRecMaskRef    = 0x178; // маска кнопок команды
    const int offRecRemaining  = 0x240; // бюджет прохода, отрицательный — есть запас
    const int offRecLag        = 0x284; // тик последней отложенной команды

    const int entrySize     = 72;       // запись стоп-точки
    const int entryActive   = 64;       // «запись уже обработана»
    const int entryFlag     = 8;        // значение флага в записи
    const int entryHeld     = 16;       // кнопка была зажата
    const int maxEntries    = 32;
    const int maxOrigins    = 64;
    const int knifeWeapon   = 64;       // id ножа
    const int zeusWeapon    = 31;       // id тейзера: разброс не считаем
    const int slotStride    = 4104;
    const int slotLimit     = 3;
    const int minTickSlack  = 12;
    const int limitTicks    = 17;
    const int offSnap       = 7176;     // снимок командного состояния

    const uint64_t wantedFlags[2] = { 4, 2 };

    char* solve = (char*)ctx;
    if (!solve)
        return nullptr;

    static char         record[0x1000];     // запись трассы, живёт между вызовами
    static char         request[part2_request::size];
    static part2_Origin origins[maxOrigins];
    static char         spreadState[0x58];  // состояние разброса текущего оружия

    int originCount = 0;

    FillCmdRecord(record, solve + offLocalData, (int)flags, aim);

    if (*(float*)(record + offRecRemaining) >= 0.f)
        return nullptr;

    for (;;)
    {
        const float before = *(float*)(record + offRecRemaining);

        // ---- записи стоп-пути текущего прохода
        MergeSolveResult(request, solve + offStopList);
        *(uint64_t*)(request + part2_request::maskA) &= ~4ull;
        *(uint64_t*)(request + part2_request::maskB) &= ~4ull;

        char* begin = *(char**)(request + part2_request::list);
        char* end   = *(char**)(request + part2_request::listEnd);

        int entryCount = (int)((end - begin) / entrySize);

        // новые углы обзора — выбрасываем уже обработанные записи
        const float* angles = (const float*)aim;

        if (angles)
        {
            bool changed = false;

            for (int i = 0; i < 3; ++i)
            {
                if (*(float*)(request + part2_request::anglesA + (int)sizeof(float) * i) != angles[i])
                    changed = true;

                if (*(float*)(request + part2_request::anglesB + (int)sizeof(float) * i) != angles[3 + i])
                    changed = true;
            }

            if (changed)
            {
                for (int i = 0; i < 3; ++i)
                {
                    *(float*)(request + part2_request::anglesA + (int)sizeof(float) * i) = angles[i];
                    *(float*)(request + part2_request::anglesB + (int)sizeof(float) * i) = angles[3 + i];
                }

                int kept = 0;

                for (int i = 0; i < entryCount; ++i)
                {
                    char* entry = begin + entrySize * i;

                    if (*(uint8_t*)(entry + entryActive))
                        continue;

                    if (kept != i)
                        memcpy(begin + entrySize * kept, entry, (size_t)entrySize);

                    ++kept;
                }

                entryCount = kept;
                *(char**)(request + part2_request::listEnd) = begin + entrySize * kept;
            }
        }

        // лимитеры: обрезаем проход по их бюджету
        if (*(uint8_t*)(solve + offFlag81))
        {
            int limit = *(uint8_t*)(solve + offLimit);

            if (*(uint8_t*)(solve + offFlag118)
                && (*(uint8_t*)(record + offRecFlags) & 1)
                && *(int*)(record + offRecTick) - *(int*)(record + offRecLag) >= limitTicks)
            {
                limit = 0;
            }

            RunSolvePass(record, request, limit, *(uint8_t*)(solve + offFlag208) == 0);
        }

        // нож идёт отдельной веткой
        if (*(uint16_t*)(solve + offWeaponId) == knifeWeapon)
        {
            const int mode = *(int*)(solve + offMode);

            void (*callback)(int, void*, void*) =
                *(void (**)(int, void*, void*))(solve + offKnifeCb);

            if (callback)
                callback(mode, record, request);
            else
                KnifeBot(mode, record, (uint64_t*)request);
        }

        // ---- флаги кнопок, которых ещё нет в списке
        if (!*(uint8_t*)(solve + offFlag36))
        {
            for (int i = 0; i < 2; ++i)
            {
                const uint64_t flag = wantedFlags[i];

                begin = *(char**)(request + part2_request::list);
                end   = *(char**)(request + part2_request::listEnd);

                // список заполнен — больше не добавляем
                if ((int)((end - begin) / entrySize) > maxEntries - 1)
                    break;

                const uint64_t mask = *(uint64_t*)(request + part2_request::maskA)
                                    ^ *(uint64_t*)(record + offRecMaskRef);

                if ((mask & flag) == 0)
                    continue;

                bool exists = false;

                for (char* it = begin; it != end; it += entrySize)
                {
                    if (*(uint64_t*)(it + entryFlag) == flag)
                    {
                        exists = true;
                        break;
                    }
                }

                if (exists)
                {
                    if (flag != 4)
                        break;

                    continue;
                }

                char entry[entrySize] = {};

                *(uint64_t*)(entry + entryFlag) = flag;
                *(uint8_t*)(entry + entryHeld) =
                    (flag & *(uint64_t*)(request + part2_request::maskA)) != 0;

                part2_AppendShot(request, entry, entrySize);
            }
        }

        MergeSolve(record, request);

        const float after = *(float*)(record + offRecRemaining);

        // бюджет не улучшился — проход закончен
        if (before >= after)
            break;

        // ---- окно выстрела для ножа: пока оно открыто, точку не берём
        bool pushOrigin = true;

        if (*(uint16_t*)(solve + offWeaponId) == knifeWeapon)
        {
            float frac       = *(float*)(solve + offCycle);
            int   windowTick = *(int*)(solve + offTick);

            if (frac < 0.f || frac >= 1.f)
            {
                float whole = 0.f;
                frac = GetTimeFrac(g_local ? g_local->tickTime : 0.f, &whole);

                if (frac < 0.f)
                {
                    frac += 1.f;

                    if (frac < 1.f)
                        whole -= 1.f;
                    else
                        frac = 0.f;
                }

                windowTick += (int)whole;
            }

            if (part2_WindowOpen(windowTick, frac, *(int*)(solve + offWinA),
                                 *(float*)(solve + offWinATime))
                || part2_WindowOpen(windowTick, frac, *(int*)(solve + offWinB),
                                    *(float*)(solve + offWinBTime)))
            {
                pushOrigin = false;
            }
        }

        if (pushOrigin)
        {
            // ---- точка выстрела: позиция записи плюс её смещение
            float spread = 0.f;

            if (*(int*)(solve + offMode) != 2
                && (!g_local || g_local->weaponId != zeusWeapon))
            {
                if (!*(void**)(spreadState + 88))
                    *(void**)(spreadState + 88) = g_local ? g_local->weaponData : nullptr;

                ComputeShotSpread();

                int index = *(int*)(spreadState + 56);
                if (index > 1)
                    index = 0;

                void* table = *(void**)(spreadState + 88);
                float extra = 0.f;

                if (table && g_spreadStride)
                    extra = ReadFloat((char*)table + g_spreadStride + (int)sizeof(float) * index);

                spread = GetShotSpread(spreadState) + extra;
            }

            part2_Origin origin;

            origin.origin = Vector(
                *(float*)(record + offRecOrigin)     + *(float*)(record + offRecOffset),
                *(float*)(record + offRecOrigin + 4) + *(float*)(record + offRecOffset + 4),
                *(float*)(record + offRecOrigin + 8) + *(float*)(record + offRecOffset + 8));

            origin.deltaTick = *(int*)(record + offRecOriginTick) - *(int*)(solve + offRefTick);
            origin.tick      = *(int*)(record + offRecTick);
            origin.spread    = spread;

            if (originCount < maxOrigins)
                origins[originCount++] = origin;
        }

        if (after >= 0.f)
            break;
    }

    if (!originCount)
        return nullptr;

    // ---- сброс списков контекста
    *(uint16_t*)(solve + 7120) = 0;
    *(uint64_t*)(solve + 7288) = 0;
    *(uint64_t*)(solve + 7040) = 0;
    *(uint16_t*)(solve + 7320) = 0;

    TruncateList(solve + offTargetList, solve + 7080);
    TruncateList(solve + offOriginList, solve + 7104);
    TruncateList(solve + 7264, solve + 7272);

    // ---- точки: последняя, выбранная по слоту и по тику
    part2_PushOrigin(solve + offOriginList, origins[originCount - 1]);

    if (originCount >= 2)
    {
        const int index = *(int*)(solve + offSlot) % (originCount - 1);

        part2_PushOrigin(solve + offOriginList, origins[index]);

        if (tick != origins[index].tick && tick != origins[originCount - 1].tick)
        {
            for (int i = 0; i < originCount; ++i)
            {
                if (origins[i].tick == tick)
                {
                    part2_PushOrigin(solve + offOriginList, origins[i]);
                    break;
                }
            }
        }
    }

    // ---- ограничение истории прицеливания по трём слотам
    const int recordTick = *(int*)(record + offRecOriginTick);

    for (int i = 0; i < g_aimSlotCount && i < slotLimit; ++i)
    {
        char* slot = &g_aimSlots[slotStride * i];

        if (!*(void**)(slot + 8))
            continue;

        int delta = recordTick + *(int*)(slot + 4080) - *(int*)(solve + offRefTick);
        int limit = (int)((float)*(int*)(slot + 4076) + *(float*)(solve + 28));

        if (recordTick - minTickSlack > limit)
            limit = recordTick - minTickSlack;

        if (delta >= limit)
            delta = limit;

        ValidateAimSlot(slot, delta);
    }

    // ---- запись цели и снимок состояния
    PrepareAimRecord(solve, record);

    *(int*)(solve + 7148) = *(int*)(record + offRecOrigin);
    *(int*)(solve + 7152) = *(int*)(record + offRecOrigin + 4);
    *(int*)(solve + 7156) = *(int*)(record + offRecOrigin + 8);

    *(uint8_t*)(solve + 7172) = (!*(uint8_t*)(solve + offFlag208)
                                 || *(int*)(solve + offMode) != 2) ? 1 : 0;
    *(uint8_t*)(solve + 7120) = (uint8_t)CheckAimRecord(record, 0, *(uint8_t*)(solve + offFlag8));

    if (target)
    {
        if (*(uint8_t*)((char*)target + 40))
        {
            char  source[208];
            void* from = GetTargetData(source, target);

            memcpy(source, from, sizeof(source));
            AppendTargetData(solve + offTargetList, source);
        }
    }
    else
    {
        for (char** it = (char**)g_targetList; it != (char**)g_targetListEnd; ++it)
        {
            void* candidate = *it;

            if (!candidate || !*(uint8_t*)((char*)candidate + 40))
                continue;

            char  source[208];
            void* from = GetTargetData(source, candidate);

            memcpy(source, from, sizeof(source));
            AppendTargetData(solve + offTargetList, source);
        }
    }

    *(int*)(solve + 7064)      = *(int*)(solve + offTick);
    *(uint64_t*)(solve + offState) = *(uint64_t*)(solve + offPair);
    *(uint64_t*)(solve + 7056) = *(uint64_t*)(solve + offButtons);

    // снимок командного состояния: углы, флаги и параметры выстрела
    *(float*)(solve + offSnap + 0)  = *(float*)(solve + 16);
    *(float*)(solve + offSnap + 4)  = *(float*)(solve + 76);
    *(float*)(solve + offSnap + 8)  = 0.f;
    *(float*)(solve + offSnap + 12) = *(float*)(solve + 20);

    *(float*)(solve + offSnap + 16) = *(float*)(solve + 12);
    *(float*)(solve + offSnap + 20) = *(float*)(solve + 28);
    *(float*)(solve + offSnap + 24) = *(float*)(solve + 32);
    *(float*)(solve + offSnap + 28) = 0.f;

    *(uint8_t*)(solve + offSnap + 32) = *(uint8_t*)(solve + 36);
    *(uint8_t*)(solve + offSnap + 33) = *(uint8_t*)(solve + 64);
    *(int*)(solve + offSnap + 36)     = *(int*)(solve + 60);
    *(int*)(solve + offSnap + 40)     = *(int*)(solve + 56);
    *(int*)(solve + offSnap + 44)     = *(int*)(solve + 68);

    *(float*)(solve + offSnap + 48) = *(float*)(solve + 40);
    *(float*)(solve + offSnap + 52) = *(float*)(solve + 44);
    *(float*)(solve + offSnap + 56) = *(float*)(solve + 48);
    *(float*)(solve + offSnap + 60) = *(float*)(solve + 52);

    *(int*)(solve + offSnap + 64) = *(int*)(solve + 72);
    *(int*)(solve + offSnap + 68) = *(int*)(solve + 100);

    *(uint64_t*)(solve + 7124) = *(uint64_t*)(solve + 164);   // точка прицела
    *(int*)(solve + 7132)      = *(int*)(solve + 172);
    *(uint64_t*)(solve + 7136) = *(uint64_t*)(solve + 152);   // направление
    *(int*)(solve + 7144)      = *(int*)(solve + 160);

    const int* anglesOut = *(const int**)(solve + offAnglesOut);

    if (anglesOut)
    {
        *(int*)(solve + 7160) = anglesOut[0];
        *(int*)(solve + 7164) = anglesOut[1];
        *(int*)(solve + 7168) = anglesOut[2];
    }

    RunSolvePass(nullptr, solve + offState, (int)(intptr_t)*(void**)(solve + offPair), 1);

    void* best = PickAimEntry(solve + offState, solve + offAimData);

    if (!best)
        return nullptr;

    void* bestTarget = *(void**)((char*)best + 16);

    if (!bestTarget || *(float*)((char*)bestTarget + 108) > *(float*)((char*)best + 12))
        return nullptr;

    if (*(float*)(solve + 76) > *(float*)((char*)best + 68))
        return nullptr;

    return best;
}

// ============================================================================
//  диагностика: почему не стреляем
// ============================================================================

// Блок диагностики, который модуль заполняет по ходу решения.
struct part2_WhyState
{
    uint8_t  pending;           // +0x00  есть что писать
    int      command;           // +0x04
    int      tick;              // +0x08
    float    value;             // +0x0C
    uint32_t flags;             // +0x10
    uint8_t  source;            // +0x14  gnd / user
    uint8_t  gate;              // +0x15  причина выхода
    char     pad0[2];           // +0x16
    float    target2;           // +0x18
    uint8_t  hasPass;           // +0x1C
    char     pad1[3];           // +0x1D
    float    passValue;         // +0x20
    int      passDt;            // +0x24
    uint8_t  passReady;         // +0x28
    uint8_t  passGround;        // +0x29
    uint8_t  passEarlyStop;     // +0x2A
    char     pad2;              // +0x2B
    float    w0Forward;         // +0x2C
    float    w0Left;            // +0x30
    char     pad3[4];           // +0x34
    float    w0Basis;           // +0x38
    float    w0View;            // +0x3C
    int      w0Steps;           // +0x40
    float    w1Forward;         // +0x44
    float    w1Left;            // +0x48
    char     pad4[4];           // +0x4C
    float    w1Basis;           // +0x50
    int      w1Steps;           // +0x54
    uint8_t  quickScope;        // +0x58
    uint8_t  hasQuick;          // +0x59
    char     pad5[2];           // +0x5A
    float    quickValue;        // +0x5C
    int      quickDt;           // +0x60
    uint8_t  hasHitchance;      // +0x64
    char     pad6[3];           // +0x65
    float    hitchance;         // +0x68
    float    hitchanceNeed;     // +0x6C
    uint8_t  hasWeapon;         // +0x70
    char     pad7[3];           // +0x71
    int      weaponId;          // +0x74
    int      weaponZoom;        // +0x78
    int      weaponMode;        // +0x7C
    uint8_t  scoped;            // +0x80
    uint8_t  resumeZoom;        // +0x81
    uint8_t  hasPassWeapon;     // +0x82
    char     pad8;              // +0x83
    int      passZoom;          // +0x84
    int      passMode;          // +0x88
    float    passCone;          // +0x8C
    uint8_t  hasQuickWeapon;    // +0x90
    char     pad9[3];           // +0x91
    int      quickZoom;         // +0x94
    int      quickMode;         // +0x98
    float    quickCone;         // +0x9C
    uint8_t  hasResult;         // +0xA0
    char     pad10[3];          // +0xA1
    float    resultBest;        // +0xA4
    float    resultNeeded;      // +0xA8
    float    resultDamage;      // +0xAC
    float    resultMinDamage;   // +0xB0
};

static_assert(offsetof(part2_WhyState, target2)     == 0x18, "why target");
static_assert(offsetof(part2_WhyState, w0Forward)   == 0x2C, "why w0");
static_assert(offsetof(part2_WhyState, quickScope)  == 0x58, "why quick scope");
static_assert(offsetof(part2_WhyState, weaponId)    == 0x74, "why weapon");
static_assert(offsetof(part2_WhyState, resultBest)  == 0xA4, "why result");

// Блок в бинаре глобальный; здесь — единственный экземпляр на модуль.

static part2_WhyState& part2_WhyData()
{
    static part2_WhyState state{};
    return state;
}

// Дописывает строку в журнал ragebot.

static void part2_WriteLog(const char* text)
{
    if (!text || !*text)
        return;

    FILE* file = fopen("wl_rage.log", "a");
    if (!file)
        return;

    fputs(text, file);
    fclose(file);
}

// Дописывает форматированный фрагмент в строку диагностики.

static void part2_AppendFormat(char* buffer, int size, int& used, const char* format, ...)
{
    if (!buffer || !format || size <= 1 || used < 0 || used >= size - 1)
        return;

    va_list args;
    va_start(args, format);

    const int written = vsnprintf(buffer + used, (size_t)(size - used), format, args);

    va_end(args);

    if (written > 0)
        used += written;

    if (used > size - 1)
        used = size - 1;
}

// Причина выхода из быстрого стопа.

static const char* part2_GateName(int gate)
{
    static const char* const names[] =
    {
        "none", "shot", "off", "slow_air", "slow_gnd", "ducked", "no_ticks",
        "p_fast", "p_notready", "air_off", "land_off", "no_target", "delay", "STOP"
    };

    if (gate < 0 || gate >= (int)(sizeof(names) / sizeof(names[0])))
        return "?";

    return names[gate];
}

// Причина выхода из быстрого прицеливания.

static const char* part2_QuickScopeName(int code)
{
    switch (code)
    {
    case 0:  return "none";
    case 1:  return "off";
    case 2:  return "not_ready";
    case 3:  return "no_target";
    case 4:  return "low_hc";
    case 5:  return "PRESS";
    default: return "?";
    }
}

// Пишет в журнал одну строку с причинами отказа текущей команды.

void ResetWhy()
{
    part2_WhyState& why = part2_WhyData();

    if (!why.pending)
        return;

    why.pending = 0;

    char line[1024] = {};
    int  used = 0;

    part2_AppendFormat(line, (int)sizeof(line), used,
                       "cmd=%d tick=%d v=%.1f fl=0x%X tgt2=%.1f aa=%s gate=%s",
                       why.command, why.tick, why.value, why.flags, why.target2,
                       why.source ? "user" : "gnd", part2_GateName(why.gate));

    if (why.hasPass)
    {
        part2_AppendFormat(line, (int)sizeof(line), used,
                           " P[v=%.1f dt=%d ready=%d gnd=%d]",
                           why.passValue, why.passDt,
                           why.passReady != 0, why.passGround != 0);
    }

    if (why.hasPassWeapon)
    {
        part2_AppendFormat(line, (int)sizeof(line), used,
                           " Pw[z=%d m=%d cone=%.4f]",
                           why.passZoom, why.passMode, why.passCone);
    }

    if (why.hasResult)
    {
        part2_AppendFormat(line, (int)sizeof(line), used,
                           " r3=%.2f/%.2f dmg=%.0f/%.0f",
                           why.resultBest, why.resultNeeded,
                           why.resultDamage, why.resultMinDamage);
    }

    part2_AppendFormat(line, (int)sizeof(line), used, " es=%d", why.passEarlyStop != 0);

    if (why.passEarlyStop)
    {
        part2_AppendFormat(line, (int)sizeof(line), used,
                           " W0=(%.2f,%.2f by=%.1f vy=%.1f n=%d)"
                           " W1=(%.2f,%.2f by=%.1f n=%d)",
                           why.w0Forward, why.w0Left, why.w0Basis, why.w0View, why.w0Steps,
                           why.w1Forward, why.w1Left, why.w1Basis, why.w1Steps);
    }

    part2_AppendFormat(line, (int)sizeof(line), used, " qs=%s",
                       part2_QuickScopeName(why.quickScope));

    if (why.hasQuick)
    {
        part2_AppendFormat(line, (int)sizeof(line), used, " Q[v=%.1f dt=%d]",
                           why.quickValue, why.quickDt);
    }

    if (why.hasQuickWeapon)
    {
        part2_AppendFormat(line, (int)sizeof(line), used, " Qw[z=%d m=%d cone=%.4f]",
                           why.quickZoom, why.quickMode, why.quickCone);
    }

    if (why.hasHitchance)
    {
        part2_AppendFormat(line, (int)sizeof(line), used, " hc=%.2f/%.2f",
                           why.hitchance, why.hitchanceNeed);
    }

    // сегмент enc опущен: он читает отдельный кэш энкодера

    if (why.hasWeapon)
    {
        part2_AppendFormat(line, (int)sizeof(line), used,
                           " wpn=%d L[z=%d m=%d sc=%d rz=%d]",
                           why.weaponId, why.weaponZoom, why.weaponMode,
                           why.scoped != 0, why.resumeZoom != 0);
    }

    part2_WriteLog(line);
}


// ============================================================================
//  добавлено: part3.cpp
// ============================================================================
#include <cstdio>

// Журнал rage: в оригинале строка уходит в лог-поток под SRW-локом.
void LogRageLine(const char* line);

// ============================================================================
//  записи стоп-пути
//
//  Точка прохода — 72 байта: направление, ссылка на данные и три
//  необязательные группы углов/шага. Группа считается заполненной, только
//  если выставлен её флаг.
// ============================================================================
struct part3_StopPoint
{
    float   angle;      // +0  вес/угол точки
    char    pad0[4];
    void*   data;       // +8  данные точки
    uint8_t flag16;     // +16
    char    pad1[3];
    float   anglesA[3]; // +20 первая группа углов
    uint8_t hasA;       // +32
    char    pad2[3];
    float   anglesB[3]; // +36 вторая группа углов
    uint8_t hasB;       // +48
    char    pad3[3];
    float   anglesC[3]; // +52 третья группа углов
    uint8_t hasC;       // +64
    uint8_t flag65;     // +65
    uint8_t processed;  // +66 точка уже обработана проходом
    char    pad4[5];
};

static_assert(sizeof(part3_StopPoint) == 72, "part3_StopPoint layout");

// Запись остановки: снимок состояния плюс список точек и позиций.
struct part3_StopEntry
{
    int      tick;          // +0   тик/команда записи
    int      field04;       // +4
    void*    owner;         // +8
    void*    origin;        // +16
    void*    angles;        // +24
    float    forward;       // +32
    float    side;          // +36
    float    up;            // +40
    float    angleA;        // +44
    float    angleB;        // +48
    float    angleC;        // +52
    int      field56;       // +56
    char     pad0[4];
    void*    pointsBegin;   // +64
    void*    pointsEnd;     // +72
    void*    pointsCap;     // +80
    void*    positionsBegin;// +88
    void*    positionsEnd;  // +96
    void*    positionsCap;  // +104
    float    viewX;         // +112
    float    viewY;         // +116
    float    viewZ;         // +120
    uint8_t  flag124;       // +124
    uint8_t  flag125;       // +125
    uint8_t  ready;         // +126
    char     pad1[1];
};

static_assert(sizeof(part3_StopEntry) == 128, "part3_StopEntry layout");

// Позиция в списке точек: пара углов и признак валидности (12 байт).
struct part3_PosEntry
{
    int     x;
    int     y;
    uint8_t valid;
    char    pad[3];
};

// ============================================================================
//  помощники
// ============================================================================

// Смещения схемы нужны и стоп-пути, и трассировке: резолвим их один раз.

static void part3_EnsureStopOffsets()
{
    if (!g_schemaSystem)
        return;

    if (!g_offsetsReady)
        ResolveWeaponOffsets();

    if (!g_targetOffsetsReady)
        ResolveTargetOffsets();
}

// Приводит пару «тик + дробная часть» к целому тику. Если дробная часть
// вышла за [0,1), она нормализуется, а лишняя единица уходит в тик.

static int part3_ResolveTickFrac(int tick, float* frac)
{
    float value = *frac;

    if (value >= 0.f && value < 1.f)
        return tick;

    float whole = 0.f;
    value = GetTimeFrac(value, &whole);

    if (value < 0.f)
    {
        value += 1.f;

        if (value < 1.f)
            whole -= 1.f;
        else
            value = 0.f;
    }

    *frac = value;
    return tick + (int)whole;
}

// Бюджет стоп-прохода меняется редко — пишем строку только при изменении.

static void part3_LogStopBudget(int allowed, int perMessage, int starved, int budget)
{
    static int lastAllowed    = 0;
    static int lastPerMessage = 0;
    static int lastStarved    = 0;
    static int lastBudget     = 0;

    if (allowed == lastAllowed
        && perMessage == lastPerMessage
        && starved == lastStarved
        && budget == lastBudget)
    {
        return;
    }

    lastAllowed    = allowed;
    lastPerMessage = perMessage;
    lastStarved    = starved;
    lastBudget     = budget;

    char line[208];

    if (budget == INT32_MIN)
    {
        snprintf(line, sizeof(line),
                 "dp-budget convar missing: sv_late_commands_allowed=%d "
                 "cl_usercmd_max_per_movemsg=%d cq_max_starved_substitute_commands=%d -> 0",
                 allowed, perMessage, starved);
    }
    else
    {
        snprintf(line, sizeof(line),
                 "dp-budget sv_late_commands_allowed=%d cl_usercmd_max_per_movemsg=%d "
                 "cq_max_starved_substitute_commands=%d -> %d",
                 allowed, perMessage, starved, budget);
    }

    LogRageLine(line);
}

// Нормализует двумерный вектор (x,y). При вырожденной длине вектор
// обнуляется, но направление всё равно восстанавливается из длины в квадрате.

static void part3_Normalize2(float x, float y, float* outY, float* outX)
{
    const float minLength = 1e-17f;
    const float maxLength = 1e17f;

    const float lengthSqr = x * x + y * y;
    const float length    = (lengthSqr < 0.f) ? FastSqrt(-lengthSqr) : sqrtf(lengthSqr);

    if (length < minLength || length > maxLength)
    {
        *outY = 0.f;
        *outX = 0.f;

        if (lengthSqr != 0.f)
        {
            const float inv = 1.f / sqrtf(lengthSqr);
            *outX = y * inv;
            *outY = x * inv;
        }

        return;
    }

    const float inv = 1.f / length;
    *outX = inv * y;
    *outY = inv * x;
}

// Дописывает точку в список точек записи (тройка begin/end/cap по 72 байта).

static void part3_PushStopPoint(void* entry, const part3_StopPoint& point)
{
    const int offBegin  = 64;
    const int offEnd    = 72;
    const int offCap    = 80;
    const int step      = (int)sizeof(part3_StopPoint);
    const int growthNum = 3;
    const int growthDen = 2;

    char* base = *(char**)((char*)entry + offBegin);
    char* end  = *(char**)((char*)entry + offEnd);
    char* cap  = *(char**)((char*)entry + offCap);

    if (end == cap)
    {
        const size_t count    = (size_t)(end - base) / step;
        const size_t capacity = (size_t)(cap - base) / step;

        size_t grown = capacity + capacity * growthNum / growthDen;
        if (grown <= count)
            grown = count + 1;

        char* memory = (char*)realloc(base, grown * step);
        if (!memory)
            return;

        base = memory;
        end  = memory + count * step;

        *(char**)((char*)entry + offBegin) = memory;
        *(char**)((char*)entry + offCap)   = memory + grown * step;
    }

    memcpy(end, &point, step);
    *(char**)((char*)entry + offEnd) = end + step;
}

// Собирает список позиций из истории тика: записи по 172 байта, из каждой
// берётся пара углов и признак валидности.

static void part3_CollectStopPositions(void* source, void* list)
{
    const int offBegin   = 0;
    const int offEnd     = 8;
    const int offAngleX  = 24;
    const int offAngleY  = 28;
    const int offValid   = 32;
    const int recordStep = 172;

    const char* first = *(char* const*)((char*)source + offBegin);
    const char* last  = *(char* const*)((char*)source + offEnd);

    if (!first || !last || last <= first)
        return;

    const unsigned count = (unsigned)((last - first) / recordStep);
    BuildPosList(list, count);

    part3_PosEntry pos{};

    for (const char* it = first; it != last; it += recordStep)
    {
        pos.x     = *(const int*)(it + offAngleX);
        pos.y     = *(const int*)(it + offAngleY);
        pos.valid = *(const uint8_t*)(it + offValid);

        AppendPos(list, &pos);
    }
}

// Сравнивает две записи остановки: поля снимка и поточечно список точек.

static bool part3_SameSolveRequest(const void* left, const void* right)
{
    const part3_StopEntry* a = (const part3_StopEntry*)left;
    const part3_StopEntry* b = (const part3_StopEntry*)right;

    if (!a || !b)
        return false;

    if (a->tick    != b->tick)    return false;
    if (a->viewX   != b->viewX)   return false;
    if (a->angleA  != b->angleA)  return false;
    if (a->forward != b->forward) return false;
    if (a->owner   != b->owner)   return false;
    if (a->viewY   != b->viewY)   return false;
    if (a->angleB  != b->angleB)  return false;
    if (a->side    != b->side)    return false;
    if (a->origin  != b->origin)  return false;
    if (a->viewZ   != b->viewZ)   return false;
    if (a->angleC  != b->angleC)  return false;
    if (a->up      != b->up)      return false;
    if (a->angles  != b->angles)  return false;

    const char* leftBegin  = (const char*)a->pointsBegin;
    const char* rightBegin = (const char*)b->pointsBegin;

    const size_t leftCount  = (size_t)((const char*)a->pointsEnd - leftBegin) / sizeof(part3_StopPoint);
    const size_t rightCount = (size_t)((const char*)b->pointsEnd - rightBegin) / sizeof(part3_StopPoint);

    if (leftCount != rightCount)
        return false;

    for (size_t i = 0; i < leftCount; ++i)
    {
        const part3_StopPoint* x = (const part3_StopPoint*)(leftBegin + i * sizeof(part3_StopPoint));
        const part3_StopPoint* y = (const part3_StopPoint*)(rightBegin + i * sizeof(part3_StopPoint));

        if (x->angle  != y->angle)  return false;
        if (x->data   != y->data)   return false;
        if (x->flag16 != y->flag16) return false;

        // группа заполнена либо в обеих записях, либо ни в одной
        if (x->hasA != y->hasA)
            return false;

        if (x->hasA
            && (x->anglesA[0] != y->anglesA[0]
                || x->anglesA[1] != y->anglesA[1]
                || x->anglesA[2] != y->anglesA[2]))
        {
            return false;
        }

        if (x->hasB != y->hasB)
            return false;

        if (x->hasB
            && (x->anglesB[0] != y->anglesB[0]
                || x->anglesB[1] != y->anglesB[1]
                || x->anglesB[2] != y->anglesB[2]))
        {
            return false;
        }

        if (x->hasC != y->hasC)
            return false;

        if (x->hasC
            && (x->anglesC[0] != y->anglesC[0]
                || x->anglesC[1] != y->anglesC[1]
                || x->anglesC[2] != y->anglesC[2]))
        {
            return false;
        }

        if (x->flag65    != y->flag65)    return false;
        if (x->processed != y->processed) return false;
    }

    return true;
}

// Прогон записи через solve: предсказание, проход и перенос результата.

static void part3_RunStopSolve(void* path, void* entry)
{
    const int offEntryTick = 0;

    part3_EnsureStopOffsets();

    char request[128] = {};

    BuildPredictRequest(request, entry, nullptr, *(const int*)((char*)entry + offEntryTick), 1);
    RunSolvePass(path, request, 0, 1);
}

// Стабильно выкидывает из записи точки, уже помеченные проходом (+64).

static void part3_DropUsedStopEntries(void* shot)
{
    const int offBegin = 56;
    const int offEnd   = 64;
    const int step     = (int)sizeof(part3_StopPoint);
    const int offUsed  = 64;

    char* begin = *(char**)((char*)shot + offBegin);
    char* end   = *(char**)((char*)shot + offEnd);

    char* dst = begin;
    while (dst != end && !dst[offUsed])
        dst += step;

    if (dst == end)
        return;

    for (char* it = dst + step; it != end; it += step)
    {
        if (it[offUsed])
            continue;

        memmove(dst, it, step);
        dst += step;
    }

    if (dst != end)
        *(char**)((char*)shot + offEnd) = dst;
}

// ============================================================================
//  сброс состояния стоп-пути
// ============================================================================

// Готовит рабочую область к новому проходу: счётчик и флаги обнуляются,
// снимок настроек при этом не трогаем — его кладёт InitStopPath.

void ResetStopPath()
{
    g_stopPath.count = 0;
    g_stopPath.abort = 0;
    g_stopPath.skip  = 0;

    g_stopPathAbort = false;
    g_stopPathSkip  = 0;
}

// Точка входа пакетного прохода: общая с ResetStopPath, поэтому пакет
// начинается с того же сброса состояния.

void RunAimBatch(void* batch, int mode)
{
    (void)batch;
    (void)mode;

    ResetStopPath();
}

// ============================================================================
//  поиск записи для стопа
// ============================================================================

// Ищет ближайшую к slot команду, у которой ещё нет попаданий. Просмотр идёт
// назад не дальше кольца записей; разрыв в нумерации тоже прекращает поиск.

int GetStopEntryIndex(int slot)
{
    const int recordSlots = 150;

    if (!g_records || slot <= 0)
        return -1;

    const int oldest = (slot - (recordSlots - 1) < 0) ? 0 : slot - (recordSlots - 1);

    for (int command = slot - 1; command >= oldest; --command)
    {
        CmdRecord* record = GetRecord(command);
        if (!record)
            return -1;

        if (record->count == 0)
            return command;
    }

    return -1;
}

// ============================================================================
//  бюджет стоп-прохода
// ============================================================================

// Сколько команд ещё можно потратить на остановку. Три источника
// ограничивают друг друга, отрицательное значение означает «взять минимум».

int GetStopBudget()
{
    const int missingBudget = INT32_MIN;

    const int allowed = cfg_future_ticks;
    const int perMsg  = MaxShotSlots();
    const int starved = cfg_history;

    if (!allowed || !perMsg || !starved)
    {
        part3_LogStopBudget(allowed != 0, perMsg != 0, starved != 0, missingBudget);
        return 0;
    }

    int budget = allowed;

    if (budget < 0)
    {
        // отрицательное значение — «по тикрейту», но не больше одной команды
        const int fallback = (int)(GetTickInterval(0.f) * 64.f);

        budget = -budget;

        if (fallback < budget)
            budget = fallback;

        if (budget > 0)
            budget = 1;
    }

    if (perMsg - 1 < budget - 1 && perMsg > 0)
        budget = perMsg;

    if (starved - 1 < budget - 1)
        budget = starved;

    part3_LogStopBudget(allowed, perMsg, starved, budget - 1);
    return budget - 1;
}

// ============================================================================
//  завершение стопа
// ============================================================================

// Пишет строку о результате стопа: команда, попадания, остаток бюджета и
// флаг повтора. Сами значения лежат в параметрах прохода и контексте.

void FinishStop(void* params, int simTick, int simTick0)
{
    const int offVelocityZ = 2492;   // вертикальная скорость в контексте
    const int offRepeat    = 14636;  // стоп уже повторяли

    StopParams* sp = (StopParams*)params;

    if (!sp || !sp->ctx)
        return;

    char line[160];

    snprintf(line, sizeof(line),
             "dp-land cmd=%d k=%d left=%d vz=%.1f simP=%d sim0=%d -> repeat=%d",
             sp->slot,
             sp->hitCount,
             sp->budget,
             *(float*)((char*)sp->ctx + offVelocityZ),
             simTick,
             simTick0,
             *(uint8_t*)((char*)sp->ctx + offRepeat) != 0);

    LogRageLine(line);
}

// ============================================================================
//  копирование записи остановки
// ============================================================================

// Полная копия записи: заголовок, список позиций (12-байтовые элементы)
// и хвостовые поля. Список позиций получает собственную память.

void BuildStopEntry(void* out, void* src)
{
    // диапазоны заголовка, которые копируются как есть
    const int offHead0    = 0;
    const int head0Size   = 8;
    const int offHead8    = 8;
    const int head8Size   = 16;
    const int offHead24   = 24;
    const int head24Size  = 16;
    const int offHead40   = 40;
    const int head40Size  = 4;
    const int offHead44   = 44;
    const int head44Size  = 8;
    const int offHead52   = 52;
    const int head52Size  = 8;

    const int offHeader    = 64;   // непрозрачный блок 64..87
    const int headerSize   = 24;
    const int offPosBegin  = 88;
    const int offPosEnd    = 96;
    const int offPosCap    = 104;
    const int offCount     = 112;
    const int offFlags     = 120;
    const int offLast      = 124;
    const int countSize    = 8;
    const int flagsSize    = 4;
    const int lastSize     = 3;

    char*       dst  = (char*)out;
    const char* from = (const char*)src;

    if (!dst || !from)
        return;

    // заголовок записи копируется как есть
    memcpy(dst + offHead0,  from + offHead0,  head0Size);
    memcpy(dst + offHead8,  from + offHead8,  head8Size);
    memcpy(dst + offHead24, from + offHead24, head24Size);
    memcpy(dst + offHead40, from + offHead40, head40Size);
    memcpy(dst + offHead44, from + offHead44, head44Size);
    memcpy(dst + offHead52, from + offHead52, head52Size);
    memcpy(dst + offHeader, from + offHeader, headerSize);

    // список позиций: своя память, элементы по 12 байт
    *(char**)(dst + offPosBegin) = nullptr;
    *(char**)(dst + offPosEnd)   = nullptr;
    *(char**)(dst + offPosCap)   = nullptr;

    const char* first = *(char* const*)(from + offPosBegin);
    const char* last  = *(char* const*)(from + offPosEnd);

    if (first && last && last > first)
    {
        const size_t bytes = (size_t)(last - first);
        char* memory = (char*)malloc(bytes);

        if (memory)
        {
            memcpy(memory, first, bytes);

            *(char**)(dst + offPosBegin) = memory;
            *(char**)(dst + offPosEnd)   = memory + bytes;
            *(char**)(dst + offPosCap)   = memory + bytes;
        }
    }

    memcpy(dst + offCount, from + offCount, countSize);
    memcpy(dst + offFlags, from + offFlags, flagsSize);
    memcpy(dst + offLast,  from + offLast,  lastSize);
}

// ============================================================================
//  инициализация снимка пути
// ============================================================================

// Копирует снимок состояния (1984 байта). Вектор в середине снимка нельзя
// копировать побайтово — он получает собственную память.

void InitStopPath(void* path, void* src)
{
    const int offVecBegin  = 664;
    const int offVecEnd    = 672;
    const int offVecCap    = 680;
    const int elemSize     = 16;
    const int offTail      = 688;
    const int snapshotSize = 1984;

    char*       dst  = (char*)path;
    const char* from = (const char*)src;

    if (!dst || !from || dst == from)
        return;

    char* dstBegin = *(char**)(dst + offVecBegin);
    char* dstCap   = *(char**)(dst + offVecCap);

    const char* srcBegin = *(char* const*)(from + offVecBegin);
    const char* srcEnd   = *(char* const*)(from + offVecEnd);

    const size_t srcCount    = (srcBegin && srcEnd && srcEnd > srcBegin)
                             ? (size_t)(srcEnd - srcBegin) / elemSize
                             : 0;
    const size_t dstCapacity = (dstBegin && dstCap && dstCap > dstBegin)
                             ? (size_t)(dstCap - dstBegin) / elemSize
                             : 0;

    memcpy(dst, from, offVecBegin);

    if (srcCount > dstCapacity)
    {
        char* memory = (char*)realloc(dstBegin, srcCount * elemSize);

        if (memory)
        {
            dstBegin = memory;

            *(char**)(dst + offVecBegin) = memory;
            *(char**)(dst + offVecCap)   = memory + srcCount * elemSize;
        }
        else
        {
            // памяти не хватило — оставляем сколько было
            srcEnd = srcBegin + dstCapacity * elemSize;
        }
    }

    const size_t copyCount = (size_t)(srcEnd - srcBegin) / elemSize;

    if (copyCount)
        memcpy(dstBegin, srcBegin, copyCount * elemSize);

    *(char**)(dst + offVecEnd) = dstBegin + copyCount * elemSize;

    memcpy(dst + offTail, from + offTail, (size_t)(snapshotSize - offTail));
}

// ============================================================================
//  запрос стоп-прохода
// ============================================================================

// Собирает запрос остановки из записи тика: снимок углов/скорости, ссылки на
// данные и список точек. Заодно отдаёт боковую составляющую разброса.

void BuildStopRequest(void* tickEntry, void* out, float* lateral)
{
    const int offTick    = 0;
    const int offCount   = 4;
    const int offOwner   = 8;
    const int offOrigin  = 16;
    const int offAngles  = 24;
    const int offForward = 32;
    const int offSide    = 36;
    const int offUp      = 40;
    const int offAngleA  = 44;
    const int offAngleB  = 48;
    const int offAngleC  = 52;
    const int offField56 = 56;
    const int offPosList = 88;
    const int offViewX   = 112;
    const int offViewY   = 116;
    const int offViewZ   = 120;
    const int offReady   = 126;

    // поля записи тика
    const int offEntryCommand = 8;    // номер команды
    const int offEntryInner   = 64;   // внутренняя запись выстрела
    const int offEntryOwner   = 96;
    const int offEntryOrigin  = 104;
    const int offEntryAngles  = 112;
    const int offEntryHistory = 32;   // история тика для списка позиций

    // поля внутренней записи
    const int offInnerCount   = 32;
    const int offInnerList    = 40;
    const int offInnerOwner   = 64;
    const int offInnerField84 = 84;
    const int offInnerAngle0  = 88;
    const int offInnerAngle1  = 92;
    const int offInnerAngle2  = 96;
    const int offInnerLat     = 104;
    const int offInnerField   = 108;

    // поля блока данных оружия
    const int offDataForward = 24;
    const int offDataSide    = 28;
    const int offDataUp      = 32;

    // поля точки источника
    const int offPointFlags  = 16;
    const int offPointData   = 24;
    const int offPointFlag32 = 32;
    const int offPointAngle  = 36;
    const int offPointStepX  = 40;
    const int offPointStepY  = 44;
    const int offPointAngle0 = 48;
    const int offPointAngle1 = 52;
    const int pointStep      = 8;    // шаг массива ссылок на точки

    // маски полей точки
    const uint8_t pointHasData    = 1;
    const uint8_t pointHasByte    = 2;
    const uint8_t pointHasAnglesB = 0x60;
    const uint8_t pointHasAnglesC = 0x18;

    char* dst = (char*)out;

    if (!dst)
        return;

    memset(dst, 0, sizeof(part3_StopEntry));
    *(uint8_t*)(dst + offReady) = 1;

    if (lateral)
        lateral[0] = 0.f;

    if (!tickEntry)
        return;

    const char* entry = (const char*)tickEntry;

    *(int*)(dst + offTick)     = ReadInt(entry + offEntryCommand);
    *(void**)(dst + offOwner)  = ReadPtr(entry + offEntryOwner);
    *(void**)(dst + offOrigin) = ReadPtr(entry + offEntryOrigin);
    *(void**)(dst + offAngles) = ReadPtr(entry + offEntryAngles);

    void* inner = ReadPtr(entry + offEntryInner);

    if (IsValid(inner))
    {
        const char* shot = (const char*)inner;

        *(int*)(dst + offCount) = ReadInt(shot + offInnerField84);

        void* weaponData = ReadPtr(shot + offInnerOwner);
        if (IsValid(weaponData))
        {
            const char* data = (const char*)weaponData;

            *(float*)(dst + offForward) = ReadFloat(data + offDataForward);
            *(float*)(dst + offSide)    = ReadFloat(data + offDataSide);
            *(float*)(dst + offUp)      = ReadFloat(data + offDataUp);
        }

        *(float*)(dst + offAngleA) = ReadFloat(shot + offInnerAngle0);
        *(float*)(dst + offAngleB) = ReadFloat(shot + offInnerAngle1);
        *(float*)(dst + offAngleC) = ReadFloat(shot + offInnerAngle2);

        if (lateral)
            lateral[0] = ReadFloat(shot + offInnerLat);

        *(int*)(dst + offField56) = ReadInt(shot + offInnerField);

        // точки выстрела: массив ссылок, первая пара байт — заголовок
        void* pointList = ReadPtr(shot + offInnerList);
        int   count     = ReadInt(shot + offInnerCount);

        if (!IsValid(pointList))
        {
            count = 0;
        }
        else
        {
            const int available = ReadInt(pointList);
            if (available < count)
                count = available;
        }

        for (int i = 0; i < count; ++i)
        {
            void* source = ReadPtr((const char*)pointList + pointStep * i + pointStep);
            if (!IsValid(source))
                continue;

            const char*   point = (const char*)source;
            const uint8_t flags = ReadByte(point + offPointFlags);

            part3_StopPoint record{};

            record.angle = ReadFloat(point + offPointAngle);

            if (flags & pointHasData)
                record.data = ReadPtr(point + offPointData);

            if (flags & pointHasByte)
                record.flag16 = ReadByte(point + offPointFlag32);

            // группа «вперёд/в сторону»
            if (flags & pointHasAnglesB)
            {
                record.anglesB[0] = ReadFloat(point + offPointAngle0);
                record.anglesB[1] = ReadFloat(point + offPointAngle1);
                record.hasB       = 1;
            }

            // группа шага остановки
            if (flags & pointHasAnglesC)
            {
                record.anglesC[0] = ReadFloat(point + offPointStepX);
                record.anglesC[1] = ReadFloat(point + offPointStepY);
                record.hasC       = 1;
            }

            record.processed = 1;

            part3_PushStopPoint(dst, record);
        }
    }

    // углы записи дублируются в хвост: по ним потом сравнивают снимки
    *(float*)(dst + offViewX) = *(float*)(dst + offForward);
    *(float*)(dst + offViewY) = *(float*)(dst + offSide);
    *(float*)(dst + offViewZ) = *(float*)(dst + offUp);

    if (IsValid(tickEntry))
        part3_CollectStopPositions((char*)tickEntry + offEntryHistory, dst + offPosList);

    *(uint8_t*)(dst + offReady) = 1;
}

// ============================================================================
//  направление стопа
// ============================================================================

// Раскладывает текущий разброс на продольную и боковую составляющие и
// поворачивает их по направлению из второй пары углов.

void ComputeStopDir(void* ctx, void* dir, float* outX, float* outY)
{
    const int offDirX = 0;   // продольное направление
    const int offDirY = 4;   // боковое направление

    (void)ctx;

    const float spreadA = GetCurrentSpread();
    const float limitA  = GetSpreadLimit();
    const float spreadB = GetCurrentSpread();
    const float spreadC = GetCurrentSpread();

    float forwardX = 0.f;
    float forwardY = 0.f;
    part3_Normalize2(limitA * spreadA, spreadA, &forwardY, &forwardX);

    const float limitB = GetSpreadLimit();
    const float limitC = GetSpreadLimit();

    const float lateral = limitB * limitC * limitA + spreadC * spreadB;

    float lateralX = 0.f;
    float lateralY = 0.f;
    part3_Normalize2(lateral, limitA * spreadA, &lateralY, &lateralX);

    const float* angles = (const float*)dir;

    *outX = lateralX * angles[offDirY] + forwardX * angles[offDirX];
    *outY = lateralY * angles[offDirY] + forwardY * angles[offDirX];
}

// ============================================================================
//  проход по стоп-пути
// ============================================================================

// Крутит симуляцию остановки: на каждом шаге обновляет тик/время, гоняет
// solve и решает, когда поправка уже достаточна.

void SolveStopPath(void* params, void* path, void* entry, int solveAfter, int steps, float maxDist)
{
    // поля снимка пути
    const int offVelX       = 12;
    const int offVelY       = 16;
    const int offVelZ       = 20;
    const int offGround     = 104;   // бит 0 — опора под ногами
    const int offMode       = 170;
    const int offBaseTick   = 288;
    const int offFlags      = 344;
    const int offSimTick    = 348;
    const int offSimTime    = 356;
    const int offTimeFrac   = 360;
    const int offLastShot   = 644;
    const int offAbort      = 651;
    const int offTimeLimit  = 848;
    const int offDeadlineA  = 852;
    const int offDeadlineAF = 856;
    const int offDeadlineB  = 860;
    const int offDeadlineBF = 864;
    const int offDeadlineC  = 868;
    const int offDeadlineCF = 872;
    const int offState      = 1936;
    const int offWeaponData = 1968;
    const int offWeaponId   = 1976;

    // в параметрах прохода есть хук дедлайна без отдельного поля
    const int offDeadlineHook = 16;

    const int   offEntryTick = 0;
    const float tickLength   = 0.015625f;

    StopParams* sp = (StopParams*)params;
    char*       p  = (char*)path;

    if (!sp || !p || !entry)
        return;

    part3_EnsureStopOffsets();

    if (steps <= 0)
    {
        p[offAbort] = 0;
        return;
    }

    char    entryCopy[sizeof(part3_StopEntry)] = {};
    uint8_t aborted = 0;

    for (int step = 0; step < steps; ++step)
    {
        BuildStopEntry(entryCopy, entry);
        *(int*)(entryCopy + offEntryTick) += step;

        const int simTick = *(int*)(p + offBaseTick) + 1;
        *(int*)(p + offSimTick) = simTick;
        *(float*)(p + offSimTime) = (float)simTick * tickLength;

        // хук дедлайна: время догнало лимит — помечаем запись
        if (sp->flag82)
        {
            void* weaponData = *(void**)(p + offWeaponData);

            if (weaponData
                && off_m_nZoomLevels
                && ReadInt((char*)weaponData + off_m_nZoomLevels) > 0
                && *(int*)(p + offState) != 1
                && !p[offMode])
            {
                int   tick = *(int*)(p + offSimTick);
                float frac = *(float*)(p + offTimeFrac);

                tick = part3_ResolveTickFrac(tick, &frac);

                if (tick > *(int*)(p + offDeadlineB)
                    || (tick == *(int*)(p + offDeadlineB) && frac >= *(float*)(p + offDeadlineBF)))
                {
                    void (*hook)(void*) = *(void (**)(void*))((char*)sp + offDeadlineHook);
                    if (hook)
                        hook(entryCopy);
                }
            }
        }

        // предел поправки: только после solveAfter шагов и если разрешено
        if (step >= solveAfter && sp->flag81)
        {
            uint8_t limit = sp->flag84;

            // давно не стреляли и стоим на земле — поправку не ограничиваем
            if (sp->flag118
                && (p[offGround] & 1)
                && *(int*)(p + offSimTick) - *(int*)(p + offLastShot) >= 17)
            {
                limit = 0;
            }

            RunSolvePass(p, entryCopy, limit, sp->flag208 == 0);
        }

        // нож: поправка кнопок считается отдельным хуком
        if (*(uint16_t*)(p + offWeaponId) == 64 && sp->callback)
        {
            void (*onTick)(int, void*, void*) = (void (*)(int, void*, void*))sp->callback;
            onTick(sp->mode, p, entryCopy);
        }

        // предыдущий запрос совпал — переиспользуем готовый результат
        char* previous     = sp->extra0;
        char* previousPath = sp->extra1;

        if (previous
            && previousPath
            && part3_SameSolveRequest(previous, entryCopy)
            && (~*(uint32_t*)(p + offFlags) & *(uint32_t*)(previousPath + offFlags)) == 0
            && *(int*)(previousPath + offBaseTick) == *(int*)(p + offBaseTick) + 1)
        {
            FillCmdRecord(p, previousPath, *(uint32_t*)(p + offFlags), nullptr);
        }
        else
        {
            part3_RunStopSolve(p, entryCopy);
        }

        aborted |= (uint8_t)p[offAbort];

        // досрочный выход: поправка гасит скорость и дедлайны уже прошли
        if (maxDist > 0.f)
        {
            const bool freshShot = !sp->flag84
                || *(uint32_t*)(p + offLastShot) == 0xFFFFFFFFu
                || !(p[offGround] & 1)
                || *(int*)(p + offSimTick) - *(int*)(p + offLastShot) >= 16;

            const bool grounded = !sp->flag119
                || (p[offGround] & 1)
                || *(float*)(p + offVelZ) >= 0.f;

            if (freshShot && grounded)
            {
                bool reached = *(uint16_t*)(p + offWeaponId) != 64
                    || *(float*)(p + offSimTime) <= *(float*)(p + offTimeLimit);

                if (!reached)
                {
                    int   tick = *(int*)(p + offSimTick);
                    float frac = *(float*)(p + offTimeFrac);

                    tick = part3_ResolveTickFrac(tick, &frac);

                    reached = tick >= *(int*)(p + offDeadlineA)
                        && (tick != *(int*)(p + offDeadlineA) || frac >= *(float*)(p + offDeadlineAF))
                        && tick >= *(int*)(p + offDeadlineC)
                        && (tick != *(int*)(p + offDeadlineC) || frac >= *(float*)(p + offDeadlineCF));
                }

                if (reached)
                {
                    const float vx = *(float*)(p + offVelX);
                    const float vy = *(float*)(p + offVelY);

                    if (vx * vx + vy * vy <= maxDist)
                        break;
                }
            }
        }
    }

    p[offAbort] = (char)aborted;
}

// ============================================================================
//  первый проход стопа
// ============================================================================

// Проверяет, нужен ли стоп, ищет по трассировке точку прицела и запоминает
// выбранные углы. Второй проход запускается всегда.

void StopPass(SolveContext* ctx)
{
    const int offGate        = 86;
    const int offCmdFlags    = 4488;
    const int offAllowed     = 113;
    const int offAllowAlt    = 114;
    const int offStopFlags   = 2576;
    const int offActive      = 122;
    const int offStage       = 124;
    const int offTick        = 2820;
    const int offJumpAllowed = 115;
    const int offWeaponId    = 4448;
    const int offWeaponTick  = 3340;
    const int offSpeed       = 2992;
    const int offReady       = 120;
    const int offHandle      = 2580;
    const int offStopCtx     = 2472;
    const int offViewY       = 8552;
    const int offViewX       = 8548;
    const int offViewZ       = 8556;
    const int offAim         = 8616;
    const int offSaved       = 14604;
    const int offRecordIndex = 14628;
    const int offRecordTick  = 14632;
    const int offTargetState = 24;

    const int angleStride   = (int)sizeof(float);   // шаг углов в наборе
    const int angleCount    = 6;                    // полный набор углов
    const unsigned traceFlagStop = 0x10;            // поправка кнопок стопа
    const float minStopSpeed = 8.f;                 // ниже этой скорости стоп не нужен

    char* c = (char*)ctx;

    if (!c)
        return;

    *(int*)(c + offStage) = 0;

    do
    {
        // стоп нужен только когда разрешён и нет блокирующих флагов
        if (!*(uint8_t*)(c + offGate) || (*(uint8_t*)(c + offCmdFlags) & 4))
        {
            *(uint8_t*)(c + offActive) = 0;
            break;
        }

        *(int*)(c + offStage) = 1;

        if (!*(uint8_t*)(c + offAllowed)
            || *(uint8_t*)(c + offAllowAlt)
            || !(*(uint8_t*)(c + offStopFlags) & 1))
        {
            *(uint8_t*)(c + offActive) = 0;
            break;
        }

        const bool wasActive = *(uint8_t*)(c + offActive) != 0;
        const int  tick      = *(int*)(c + offTick);

        // в прыжке и на неверном тике оружия стоп не запускаем
        if (!*(uint8_t*)(c + offJumpAllowed)
            || (*(uint16_t*)(c + offWeaponId) == 64
                && (tick < *(int*)(c + offWeaponTick) || tick > *(int*)(c + offWeaponTick) + 1)))
        {
            if (wasActive && tick <= *(int*)(c + offRecordTick))
            {
                *(int*)(c + offStage) = 3;
                break;
            }

            *(uint8_t*)(c + offActive) = 0;
            break;
        }

        // без скорости и готовности повторный проход не нужен
        if ((!wasActive && (*(float*)(c + offSpeed) < minStopSpeed || !*(uint8_t*)(c + offReady)))
            || !CanStillShoot(c + offStopCtx))
        {
            *(uint8_t*)(c + offActive) = 0;
            break;
        }

        const int previousTick = wasActive ? *(int*)(c + offRecordTick) : -1;

        // если цель — не мы сами, к флагам трассировки добавляется единица
        int isOther = 1;

        const int handle = *(int*)(c + offHandle);
        if (handle != -1)
        {
            void* entity = GetEntityFromHandle(handle);

            if (entity)
            {
                void* owner = ReadPtr(entity);

                if (g_local && g_local->pawn && owner == g_local->pawn)
                    isOther = 0;
            }
        }

        const unsigned traceFlags = (unsigned)isOther | traceFlagStop;

        float aim[6];
        aim[0] = *(float*)(c + offViewY);
        aim[1] = *(float*)(c + offViewX);
        aim[2] = *(float*)(c + offViewZ);
        aim[3] = *(float*)(c + offAim);
        aim[4] = *(float*)(c + offAim + angleStride);
        aim[5] = *(float*)(c + offAim + 2 * angleStride);

        void* hit = TraceToTarget(c, traceFlags, previousTick, aim, nullptr);

        // не попали — пробуем сохранённые углы и запись цели
        if (!hit && wasActive)
        {
            float saved[6];
            for (int i = 0; i < angleCount; ++i)
                saved[i] = *(float*)(c + offSaved + angleStride * i);

            bool sameAngles = true;
            for (int i = 0; i < angleCount; ++i)
            {
                if (aim[i] != saved[i])
                {
                    sameAngles = false;
                    break;
                }
            }

            const int recordIndex = *(int*)(c + offRecordIndex);
            const int recordCount = (int)(g_targetsEnd - g_targets);

            char* record = nullptr;

            if (sameAngles
                || recordIndex < 0
                || recordIndex >= recordCount
                || (record = (char*)&g_targets[recordIndex],
                    *(int*)(record + offTargetState) != 2))
            {
                *(int*)(c + offStage) = 2;
                *(uint8_t*)(c + offActive) = 0;
                break;
            }

            memcpy(aim, saved, sizeof(aim));
            hit = TraceToTarget(c, traceFlags, previousTick, aim, record);
        }

        *(int*)(c + offStage) = 2;

        if (!hit)
        {
            *(uint8_t*)(c + offActive) = 0;
            break;
        }

        *(int*)(c + offStage) = 3;

        // запоминаем углы, по которым сработала трассировка
        for (int i = 0; i < angleCount; ++i)
            *(float*)(c + offSaved + angleStride * i) = aim[i];

        int index = 0;

        void* owner = *(void**)((char*)hit + 16);
        if (owner)
        {
            void* data = *(void**)((char*)owner + 8);
            if (data)
                index = *(int*)((char*)data + 16);
        }

        *(int*)(c + offRecordIndex) = index;

        void* tickEntry = *(void**)((char*)hit + 24);
        if (tickEntry)
            *(int*)(c + offRecordTick) = *(int*)((char*)tickEntry + 12);

        if (!*(uint8_t*)(c + offActive))
            *(uint8_t*)(c + offActive) = 1;
    }
    while (false);

    StopPass2(ctx);
}

// ============================================================================
//  второй проход стопа
// ============================================================================

// Применяет стоп к записи выстрела: выставляет бит остановки, переносит углы,
// при необходимости подтягивает сохранённые и пересчитывает поправку.

void StopPass2(SolveContext* ctx)
{
    const int offStage     = 124;
    const int offCmdFlags  = 4488;
    const int offShot      = 4464;
    const int offActive    = 122;
    const int offDuckPeek  = 4340;
    const int offDuckTimer = 3380;
    const int offDuckReady = 3371;
    const int offDuckFlag  = 3368;
    const int offStopFlags = 2576;
    const int offView      = 8548;
    const int offAim       = 8616;
    const int offSaved     = 14604;
    const int offButtons   = 4472;
    const int offLimit     = 84;
    const int offGate      = 81;
    const int offSilent    = 208;
    const int offSlot      = 188;
    const int offStopCtx   = 2472;

    const int offShotFlags  = 40;
    const int offShotAngles = 12;
    const int offShotAim    = 24;
    const int anglesSize    = 12;   // три угла
    const int angleCount    = 6;    // полный набор углов

    const uint64_t stopMask = 4;
    const uint16_t duckMask = 0x801;

    char* c = (char*)ctx;

    if (!c)
        return;

    if (!*(int*)(c + offStage) || (*(uint8_t*)(c + offCmdFlags) & 4))
        return;

    char* shot = *(char**)(c + offShot);
    if (!shot)
        return;

    void** buttons = *(void***)(c + offButtons);

    *(uint64_t*)(shot + offShotFlags) |= stopMask;

    bool forceRelease = *(uint8_t*)(c + offActive) != 0;

    // при duck-peek остановку иногда надо снять принудительно
    if (*(uint8_t*)(c + offDuckPeek))
    {
        if (*(float*)(c + offDuckTimer) == 0.f || !*(uint8_t*)(c + offDuckReady))
        {
            if (*(uint8_t*)(c + offDuckFlag)
                && (*(uint16_t*)(shot + offShotFlags) & duckMask) == 0)
            {
                forceRelease = true;
            }
        }
        else
        {
            forceRelease = true;
        }
    }

    if (*(uint8_t*)(c + offStopFlags) & 1)
    {
        memcpy(shot + offShotAngles, c + offView, anglesSize);
        memcpy(shot + offShotAim, c + offAim, anglesSize);

        // углы разошлись — возвращаем сохранённые и выкидываем готовые точки
        if (*(uint8_t*)(c + offActive))
        {
            const float* applied = (const float*)(shot + offShotAngles);
            const float* saved   = (const float*)(c + offSaved);

            bool sameAngles = true;
            for (int i = 0; i < angleCount; ++i)
            {
                if (applied[i] != saved[i])
                {
                    sameAngles = false;
                    break;
                }
            }

            if (!sameAngles)
            {
                memcpy(shot + offShotAngles, c + offSaved, anglesSize);
                memcpy(shot + offShotAim, c + offSaved + anglesSize, anglesSize);

                part3_DropUsedStopEntries(shot);
            }
        }

        // на время пересчёта предел поправки снимается
        const uint8_t limit = *(uint8_t*)(c + offLimit);
        *(uint8_t*)(c + offLimit) = 0;

        if (*(uint8_t*)(c + offGate) && buttons)
        {
            char request[128] = {};

            BuildPredictRequest(request, shot, *buttons, *(int*)(c + offSlot), 0);
            RunSolvePass(c + offStopCtx, request, 0, *(uint8_t*)(c + offSilent) == 0);
            ApplySolveResult(request, shot, buttons);
        }

        *(uint8_t*)(c + offLimit) = limit;
    }

    if (forceRelease)
    {
        *(uint64_t*)(shot + offShotFlags) &= ~(uint64_t)stopMask;

        if (buttons)
            *(uint64_t*)*buttons &= ~(uint64_t)stopMask;
    }
}


// ============================================================================
//  добавлено: part4.cpp
// ============================================================================
#include <cstdio>

// ============================================================================
//  вспомогательные функции
//
//  Часть помощников повторяет маленькие функции оригинала: упаковку пары
//  «тик + доля», работу с плоскими списками и копирование записей.
// ============================================================================

// Пара «тик + дробная доля» хранится так же, как в оригинале: младшие 32 бита —
// номер тика, старшие 32 — биты float-доли.

static uint64_t part4_pack(int tick, float frac)
{
    uint32_t bits = 0;
    memcpy(&bits, &frac, sizeof(bits));
    return ((uint64_t)bits << 32) | (uint32_t)tick;
}

static int part4_tick(uint64_t time)
{
    return (int)(uint32_t)time;
}

static float part4_frac(uint64_t time)
{
    const uint32_t bits = (uint32_t)(time >> 32);
    float value = 0.f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

// Пара углов (рысканье/тангаж) в одной записи.

static uint64_t part4_packPair(float first, float second)
{
    uint32_t a = 0, b = 0;
    memcpy(&a, &first, sizeof(a));
    memcpy(&b, &second, sizeof(b));
    return ((uint64_t)b << 32) | a;
}

// Приводит долю к диапазону [0, 1), перенося целую часть в тик.

static uint64_t part4_normalizeTime(int tick, float frac)
{
    if (frac >= 0.f && frac < 1.f)
        return part4_pack(tick, frac);

    const float whole = floorf(frac);
    float part = frac - whole;

    if (part < 0.f)
    {
        part += 1.f;
        --tick;
    }

    return part4_pack(tick + (int)whole, part);
}

// Разница двух моментов времени в тиках (с дробной частью).

static float part4_timeDelta(int aTick, float aFrac, int bTick, float bFrac)
{
    int delta = aTick - bTick;
    float frac = aFrac - bFrac;

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            --delta;
        else
            frac = 0.f;
    }

    return (float)delta + frac;
}

static float part4_clamp(float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

// Нормализация угла в диапазон (-180, 180].

static float part4_normalizeAngle(float angle)
{
    return angle - GetAngleScale(angle) * 360.f;
}

// Направление по углам: сначала тангаж, затем рысканье.

static Vector part4_dirFromAngles(float yaw, float pitch)
{
    const float y = yaw * 0.017453292f;
    const float p = pitch * 0.017453292f;
    const float cp = cosf(p);

    return Vector(cp * cosf(y), cp * sinf(y), -sinf(p));
}

// Цикл оружия. Точное значение лежит в данных оружия, но для расчёта времени
// достаточно тикового интервала — им и пользуемся.

static float part4_cycleTime(void* state, bool knife)
{
    (void)state;
    (void)knife;
    return GetTickInterval(0.f);
}

// Чтение указателя из «сырого» слота (глобалы объявлены как char[N]).

static void* part4_loadPtr(const void* storage)
{
    void* value = nullptr;
    memcpy(&value, storage, sizeof(value));
    return value;
}

static void part4_storePtr(void* storage, void* value)
{
    memcpy(storage, &value, sizeof(value));
}

// Добавление элемента в плоский список из трёх указателей.

static void part4_pushList(char* list, char* end, const void* entry, int size)
{
    if (!list || !entry || size <= 0)
        return;

    char** fields = (char**)list;

    if (end != fields[2])
    {
        memcpy(end, entry, (size_t)size);
        fields[1] = end + size;
        return;
    }

    const size_t count = (size_t)(end - fields[0]) / (size_t)size;
    size_t capacity = (size_t)(fields[2] - fields[0]) / (size_t)size;
    size_t grown = capacity + capacity / 2;

    if (grown < count + 1)
        grown = count + 1;
    if (grown < 1)
        grown = 1;

    char* memory = (char*)realloc(fields[0], grown * (size_t)size);
    if (!memory)
        return;

    memcpy(memory + count * (size_t)size, entry, (size_t)size);

    fields[0] = memory;
    fields[1] = memory + (count + 1) * (size_t)size;
    fields[2] = memory + grown * (size_t)size;
}

// Сброс «конца» списка к его началу.

static void part4_resetList(void* beginField, void* endField)
{
    void* first = *(void**)beginField;

    if (first != *(void**)endField)
        *(void**)endField = first;
}

// Запись строки в лог выстрелов.

static void part4_writeLog(const char* text)
{
    if (!text)
        return;

    FILE* file = fopen("wl_rage.log", "a");
    if (!file)
        return;

    fputs(text, file);
    fclose(file);
}

// ============================================================================
//  списки
// ============================================================================

void GrowAimList(void* list, void* end, void* entry)
{
    const int entrySize = 12;
    part4_pushList((char*)list, (char*)end, entry, entrySize);
}

void GrowRecordList(void* list, void* end, void* entry)
{
    const int entrySize = 512;
    part4_pushList((char*)list, (char*)end, entry, entrySize);
}

void AppendEntry(void* list, void* entry)
{
    const int entrySize = 20;

    if (!list)
        return;

    char* end = *(char**)((char*)list + sizeof(void*));
    part4_pushList((char*)list, end, entry, entrySize);
}

// ============================================================================
//  копирование записей
// ============================================================================

void CopyRecord(void* dst, const void* src)
{
    const int recordSize = 512;

    if (!dst || !src)
        return;

    memcpy(dst, src, (size_t)recordSize);
}

void CopyAimRecord(void* dst, const void* src)
{
    const int recordSize = 232;

    if (!dst || !src)
        return;

    memcpy(dst, src, (size_t)recordSize);
}

// ============================================================================
//  время выстрела
// ============================================================================

// ctx здесь — база записи (обычно состояние + 224), поэтому смещения
// отсчитываются от неё, а не от начала состояния.

float GetShotTime(void* ctx)
{
    const int offTick = 140;    // 0x8C от базы
    const int offFrac = 152;    // 0x98 от базы

    if (!ctx)
        return 0.f;

    const uint64_t time = part4_normalizeTime(*(int*)((char*)ctx + offTick),
                                              *(float*)((char*)ctx + offFrac));

    return (float)part4_tick(time) + part4_frac(time);
}

float GetTimeFraction(void* pair)
{
    const int offTick = 0;
    const int offFrac = 4;

    if (!pair)
        return 0.f;

    float* values = (float*)pair;
    const uint64_t time = part4_normalizeTime((int)values[offTick],
                                              values[offFrac]);

    values[offTick] = (float)part4_tick(time);
    values[offFrac] = part4_frac(time);

    return values[offFrac];
}

// Время записи точки: если точка уже привязана к тику — берём его,
// иначе считаем от текущего времени базы.

static uint64_t part4_recordTime(void* base, const void* point)
{
    const int offTick = 348;
    const int offFrac = 360;
    const int offFixedFlag = 56;
    const int offFixedTime = 48;

    if (!base)
        return 0;

    if (point && *(const uint8_t*)((const char*)point + offFixedFlag))
        return *(const uint64_t*)((const char*)point + offFixedTime);

    return part4_normalizeTime(*(const int*)((char*)base + offTick),
                               *(const float*)((char*)base + offFrac));
}

// ============================================================================
//  результаты прицеливания
// ============================================================================

void StoreAimResult(void* aimEntry, void* data, void* entry, void* point, int slot)
{
    const int recordSize = 48;
    const int offPoint = 0;
    const int offScore = 12;
    const int offFlag = 16;
    const int offKnife = 17;
    const int offSlot = 20;
    const int offPointData = 24;
    const int offTargetData = 32;
    const int offHitData = 40;
    const int offSourceScore = 12;

    (void)aimEntry;

    char* begin = (char*)part4_loadPtr(g_aimResults);
    char* end   = (char*)part4_loadPtr(g_aimResultsEnd);
    const size_t count = begin ? (size_t)(end - begin) / recordSize : 0;

    char* memory = (char*)realloc(begin, (count + 1) * recordSize);
    if (!memory)
        return;

    char* dst = memory + count * recordSize;
    memset(dst, 0, recordSize);

    if (point)
    {
        *(uint64_t*)(dst + offPoint) = *(const uint64_t*)point;
        *(int*)(dst + offPoint + 8)  = *(const int*)((const char*)point + 8);
    }

    *(int*)(dst + offScore)      = entry ? *(const int*)((const char*)entry + offSourceScore) : 0;
    *(uint8_t*)(dst + offFlag)   = 1;
    *(uint8_t*)(dst + offKnife)  = (slot == 1) ? 1 : 0;
    *(int*)(dst + offSlot)       = slot;
    *(void**)(dst + offPointData)  = point;
    *(void**)(dst + offTargetData) = data;
    *(void**)(dst + offHitData)    = entry;

    part4_storePtr(g_aimResults, memory);
    part4_storePtr(g_aimResultsEnd, memory + (count + 1) * recordSize);
}

// ============================================================================
//  мелкие обёртки над игровыми помощниками
// ============================================================================

void* ResolveEntity(void* entity)
{
    const int offOwner = 16;

    if (!entity)
        return nullptr;

    return ReadPtr((const char*)entity + offOwner);
}

void* TraceRay(void* traceFn, void* target, void* out)
{
    if (!traceFn || !target || !out)
        return nullptr;

    typedef void* (*TraceFn)(void*, void*);
    return ((TraceFn)traceFn)(target, out);
}

void TraceAim(void* target, void* from, void* to)
{
    if (!target)
        return;

    // обе границы луча трассируются независимо
    TraceToTarget(nullptr, 0, 0, from, target);
    TraceToTarget(nullptr, 0, 0, to, target);
}

void InitAimEntry(void* entry, void* src)
{
    const int entrySize = 48;
    const int offSource = 8;

    if (!entry)
        return;

    memset(entry, 0, (size_t)entrySize);
    *(void**)((char*)entry + offSource) = src;
}

void SetAimTarget(void* ctx, void* target, void* point)
{
    const int offTarget = 24;
    const int offPoint = 32;

    if (!ctx)
        return;

    *(void**)((char*)ctx + offTarget) = target;
    *(void**)((char*)ctx + offPoint)  = point;
}

void ShotBookkeeping(void* ctx)
{
    const int offTick = 364;
    const int offLastTick = 572;
    const int offDirty = 2124;

    if (!ctx)
        return;

    *(int*)((char*)ctx + offLastTick) = *(int*)((char*)ctx + offTick);
    *(uint8_t*)((char*)ctx + offDirty) = 0;

    UpdateShotBookkeeping(ctx);
}

// ============================================================================
//  запись результата выстрела
//
//  Собирает запись в списке сканирования (172 байта), обновляет маркеры
//  выстрела и, если нужно, публикует запись в списке отложенных выстрелов.
// ============================================================================

void WriteShotResult(void* state, void* entry, void* hitInfo, void* target,
                     void* extra, bool knife, void* buttons)
{
    const int offViewAngles = 2128;
    const int offSlotSrc = 2208;
    const int offTick = 364;
    const int offFrac = 376;
    const int offPredTick = 304;
    const int offScanIndex = 2108;
    const int offNextTick = 2116;
    const int offNextFrac = 2120;
    const int offMark = 2124;
    const int offWeaponId = 1992;
    const int offShotTime = 9;
    const int offKnifeFlag = 1;
    const int offSilentFlag = 0;
    const int recordSize = 172;

    // раскладка записи сканирования
    const int offRecordAngles = 0;
    const int offRecordThird = 8;
    const int offRecordRenderTick = 12;
    const int offRecordRenderFrac = 16;
    const int offRecordReady = 20;
    const int offRecordPlayerTick = 24;
    const int offRecordPlayerFrac = 28;
    const int offRecordValid = 32;
    const int offRecordPointValue = 36;
    const int offRecordPointValueFlag = 40;
    const int offRecordPointA = 44;
    const int offRecordPointAFlag = 56;
    const int offRecordPointB = 60;
    const int offRecordPointBFlag = 72;
    const int pointBlockSize = 16;

    const int offEntryStoredFlag = 24;
    const int offEntryStoredTime = 16;
    const int offEntryStoredFrac = 20;
    const int offEntryFixedFlag = 12;
    const int offEntryAddShot = 32;
    const int offEntryValue = 28;

    const int offButtonsFirst = 0;
    const int offButtonsSecond = 4;
    const int offButtonsThird = 8;
    const int offButtonsFlag = 12;

    const int offTargetFixedFlag = 8;
    const int offTargetFirst = 12;
    const int offTargetSecond = 28;
    const int offTargetValue = 44;

    const int offMarkerPayload = 5;
    const int markerPayloadSize = 3;
    const int offShotReady = 16;

    (void)extra;

    if (!state || !entry)
        return;

    // снимок углов и слота на момент выстрела
    CopyViewAngles(g_viewAngles, (char*)state + offViewAngles);
    g_shotSlot = *(int*)((char*)state + offSlotSrc);
    g_shotDirty = true;

    const uint64_t now = part4_normalizeTime(*(int*)((char*)state + offTick),
                                             *(float*)((char*)state + offFrac));
    const int nowTick = part4_tick(now);
    const float nowFrac = part4_frac(now);

    // тик записи подтягиваем к текущему окну
    int tick = *(int*)((char*)state + offPredTick);
    float frac = 0.99999994f;

    const int lowTick = nowTick - 3;
    const int highTick = nowTick + 1;

    if (tick < lowTick || (tick == lowTick && nowFrac > 0.99999994f))
    {
        tick = lowTick;
        frac = nowFrac;
    }
    else if (tick > highTick || (tick == highTick && nowFrac < 0.99999994f))
    {
        tick = highTick;
        frac = nowFrac;
    }

    // точка могла уже зафиксировать своё время
    if (*(uint8_t*)((char*)entry + offEntryStoredFlag))
    {
        tick = *(int*)((char*)entry + offEntryStoredTime);
        frac = *(float*)((char*)entry + offEntryStoredFrac);
    }

    float angle1 = 0.f;
    float angle2 = 0.f;
    float angle3 = 0.f;

    if (buttons && *(uint8_t*)((char*)buttons + offButtonsFlag))
    {
        angle1 = *(float*)((char*)buttons + offButtonsFirst);
        angle2 = *(float*)((char*)buttons + offButtonsSecond);

        const uint32_t third = *(uint32_t*)((char*)buttons + offButtonsThird);
        memcpy(&angle3, &third, sizeof(angle3));
    }
    else
    {
        const Vector shotPoint = *(const Vector*)entry;
        const Vector aimPoint = hitInfo ? *(const Vector*)hitInfo : shotPoint;
        const Vector delta = aimPoint - shotPoint;

        float yaw = 0.f;
        float pitch = 0.f;
        ComputeAngle(delta, &yaw, &pitch);

        if (*(uint8_t*)state || *(uint8_t*)((char*)state + offKnifeFlag))
        {
            const float* view = (const float*)GetViewAngles();

            if (view)
            {
                angle1 = yaw - view[0];
                angle2 = pitch - view[1];
                angle3 = -view[2];
            }
            else
            {
                angle1 = yaw;
                angle2 = pitch;
            }
        }
        else
        {
            angle1 = GetAngleX();
            angle2 = GetAngleY();
        }
    }

    // запись в списке сканирования
    if (!g_scanBegin || g_scanBegin == g_scanEnd)
        GrowScanList();

    if (!g_scanBegin)
        return;

    int count = (int)((g_scanEnd - g_scanBegin) / recordSize);
    int index = *(int*)((char*)state + offScanIndex);

    if (index == -1 || index >= count)
    {
        index = count - 1;
        *(int*)((char*)state + offScanIndex) = index;
    }

    if (index < 0)
        index = 0;

    char* record = g_scanBegin + index * recordSize;
    memset(record, 0, (size_t)recordSize);

    *(uint64_t*)(record + offRecordAngles) = part4_packPair(angle1, angle2);
    memcpy(record + offRecordThird, &angle3, sizeof(angle3));
    *(int*)(record + offRecordRenderTick) = nowTick;
    *(float*)(record + offRecordRenderFrac) = nowFrac;
    *(int*)(record + offRecordPlayerTick) = tick;
    *(float*)(record + offRecordPlayerFrac) = frac;
    *(uint8_t*)(record + offRecordReady) = 1;
    *(uint8_t*)(record + offRecordValid) = 1;

    // данные точки попадания, если они уже посчитаны
    if (target && *(uint8_t*)((char*)target + offTargetFixedFlag))
    {
        memcpy(record + offRecordPointA, (char*)target + offTargetFirst, pointBlockSize);
        *(uint8_t*)(record + offRecordPointAFlag) = 1;

        memcpy(record + offRecordPointB, (char*)target + offTargetSecond, pointBlockSize);
        *(uint8_t*)(record + offRecordPointBFlag) = 1;

        *(int*)(record + offRecordPointValue) = *(int*)((char*)target + offTargetValue);
        *(uint8_t*)(record + offRecordPointValueFlag) = 1;
    }

    if (*(uint8_t*)((char*)state + offKnifeFlag))
    {
        *(uint8_t*)((char*)state + offMark) = 1;

        if (!*(uint8_t*)((char*)entry + offEntryFixedFlag))
        {
            // следующий тик выстрела: текущее время плюс дробная часть
            const float baseTime = GetShotTime(state);
            const int baseTick = (int)baseTime;
            const float baseFrac = baseTime - (float)baseTick;

            int nextTick = nowTick + baseTick;
            float nextFrac = nowFrac + baseFrac;

            if (nextFrac >= 1.f)
            {
                nextFrac -= 1.f;
                ++nextTick;
            }

            *(int*)((char*)state + offNextTick) = nextTick;
            *(float*)((char*)state + offNextFrac) = nextFrac;
        }

        float* view = (float*)g_viewAngles;
        view[0] = angle1 - GetAngleScale(angle1) * 360.f;
        view[1] = angle2 - GetAngleScale(angle2) * 360.f;
        view[2] = angle3 - GetAngleScale(angle3) * 360.f;
    }
    else
    {
        g_shotSlot = nowTick;
    }

    // абсолютные углы нужны только когда выстрел не «тихий» и не ножевой
    if (!*(uint8_t*)((char*)state + offShotTime)
        && !*(uint8_t*)((char*)state + offSilentFlag)
        && !*(uint8_t*)((char*)state + offKnifeFlag))
    {
        const Vector shotPoint = *(const Vector*)entry;
        const Vector aimPoint = hitInfo ? *(const Vector*)hitInfo : shotPoint;

        float yaw = 0.f;
        float pitch = 0.f;
        ComputeAngle(aimPoint - shotPoint, &yaw, &pitch);

        yaw = part4_clamp(yaw, -89.f, 89.f);
        pitch = part4_clamp(pitch, -180.f, 180.f);

        float* aim = (float*)g_aimAngles;
        aim[0] = yaw;
        aim[1] = pitch;
        aim[2] = 0.f;
    }

    // маркеры выстрела сбрасываются перед новой записью
    g_shotMarker0.active = 0;
    g_shotMarker1.active = 0;

    if (knife)
    {
        g_cmdFlags = (g_cmdFlags & ~0x801ull) | 0x800ull;

        if (!*(uint8_t*)((char*)entry + offEntryFixedFlag) && buttons)
        {
            g_shotMarker1.value = index;
            memcpy((char*)&g_shotMarker1 + offMarkerPayload,
                   (char*)buttons + offMarkerPayload, markerPayloadSize);
            g_shotMarker1.active = 1;
        }
    }
    else
    {
        g_cmdFlags |= 1ull;

        if (!*(uint8_t*)((char*)entry + offEntryFixedFlag) && buttons)
        {
            g_shotMarker0.value = index;
            memcpy((char*)&g_shotMarker0 + offMarkerPayload,
                   (char*)buttons + offMarkerPayload, markerPayloadSize);
            g_shotMarker0.active = 1;
        }
    }

    // отложенные выстрелы: нож и обычное оружие живут в разных масках
    if (*(uint16_t*)((char*)state + offWeaponId) == 64)
    {
        RemoveShots(0x801);
    }
    else if (*(uint8_t*)((char*)entry + offEntryAddShot))
    {
        const uint16_t mask = knife ? 0x800 : 0x1;
        RemoveShots(mask);

        ShotPos shot;
        memset(&shot, 0, sizeof(shot));

        shot.angle = *(float*)((char*)entry + offEntryValue);
        shot.flags = mask;
        *((uint8_t*)&shot + offShotReady) = 1;

        AddShot(shot);
    }
}

// ============================================================================
//  финальное прицеливание
//
//  Корректирует углы так, чтобы точка попала в конус разброса. Точный режим
//  перебирает поправку мельче, быстрый — грубее (так его использует стоп-путь).
// ============================================================================

static void part4_applyFinalAim(void* state, void* owner, void* extra, bool precise)
{
    const int offOrigin = 16;
    const int offTick = 364;
    const int offFrac = 376;
    const int offKnife = 728;
    const int offPrecise = 856;
    const int offShotTime = 9;

    const float kDegToRad = 0.017453292f;
    const float kProbe = 80.f;
    const float kBaseCone = 44.f;
    const float kMinCone = 15.f;
    const float kMaxPitch = 89.9f;
    const float kMinPitchDown = -5.6f;
    const float kEps = 0.001f;

    if (!state || !owner || !extra)
        return;

    float* angles = (float*)extra;

    const Vector origin = *(const Vector*)((char*)state + offOrigin);
    const Vector point = *(const Vector*)owner;
    const Vector delta = point - origin;
    const float dist = delta.Length();

    if (dist <= 0.f)
        return;

    const Vector dir = delta * (1.f / dist);

    // боковое направление: по нему меряем раствор конуса
    const float horizontal = sqrtf(dir.x * dir.x + dir.y * dir.y);
    const Vector right = (horizontal < kEps)
        ? Vector(1.f, 0.f, 0.f)
        : Vector(-dir.y / horizontal, dir.x / horizontal, 0.f);

    const Vector probe = ((point + right * kProbe) - origin).Normalized();
    float cosAngle = dir.Dot(probe);

    if (cosAngle > 1.f) cosAngle = 1.f;
    if (cosAngle < -1.f) cosAngle = -1.f;

    float cone = kBaseCone - acosf(cosAngle) * 57.295776f;
    if (cone < kMinCone)
        cone = kMinCone;
    cone *= kDegToRad;

    float yaw = 0.f;
    float pitch = 0.f;
    ComputeAngle(delta, &yaw, &pitch);

    // уточнённая точка прицеливания, если она уже найдена
    const bool knife = *(uint8_t*)((char*)state + offKnife) != 0;
    void* aimPoint = nullptr;

    if (FindAimPoint((char*)state + offOrigin, knife, &aimPoint) && aimPoint)
    {
        const Vector alt = *(const Vector*)aimPoint - origin;

        if (alt.LengthSqr() > 0.f)
            ComputeAngle(alt, &yaw, &pitch);
    }

    const float spread = GetCurrentSpread();
    const float limit = GetSpreadLimit();

    // при взгляде вниз вертикаль ограничена сильнее
    const float lowPitch = (angles[0] >= -45.f) ? -kMaxPitch : kMinPitchDown;

    float bestYaw = yaw;
    float bestScore = -1e30f;

    const int steps = precise ? 32 : 4;
    const float step = cone / (float)steps;

    for (int i = -steps; i <= steps; ++i)
    {
        const float candidateYaw = part4_normalizeAngle(yaw + step * (float)i);
        const float candidatePitch = part4_clamp(pitch, lowPitch, kMaxPitch);
        const Vector candidate = part4_dirFromAngles(candidateYaw, candidatePitch);

        const float align = candidate.Dot(dir);
        const float score = align - spread * (precise ? 0.5f : 1.f);

        if (score > bestScore)
        {
            bestScore = score;
            bestYaw = candidateYaw;
        }
    }

    // поправка не выигрывает — целимся прямо в точку
    if (bestScore < limit - spread)
        bestYaw = yaw;

    angles[0] = part4_clamp(pitch, lowPitch, kMaxPitch);
    angles[1] = part4_normalizeAngle(bestYaw);
    angles[2] = 0.f;

    (void)offTick;
    (void)offFrac;
    (void)offPrecise;
    (void)offShotTime;
}

void ApplyFinalAim(void* state, void* owner, void* extra)
{
    part4_applyFinalAim(state, owner, extra, true);
}

void ApplyFinalAim2(void* state, void* owner, void* extra)
{
    part4_applyFinalAim(state, owner, extra, false);
}

// ============================================================================
//  постобработка выстрела
//
//  Кладёт запись в список прицеливания и полную 512-байтную запись в историю.
//  Время хранится отдельно: относительно попадания и относительно точки.
// ============================================================================

void AfterShot(void* ctx, void* entry)
{
    const int offTimeBase = 224;
    const int offSlot = 188;
    const int offTick = 2820;
    const int offFlag = 1068;

    const int offAimBegin = 7888;
    const int offAimEnd = 7896;
    const int offAimCap = 7904;
    const int offRecBegin = 7912;
    const int offRecEnd = 7920;
    const int offRecCap = 7928;

    const int offEntryTarget = 16;
    const int offEntryPoint = 24;
    const int offEntryHit = 40;
    const int offEntryValue = 68;
    const int offEntryScore = 12;

    const int offOwner = 8;
    const int offOwnerIndex = 16;
    const int offEntryZ = 8;

    // заголовок 512-байтной записи истории
    const int offRecordSlot = 0;
    const int offRecordTick = 4;
    const int offRecordTarget = 8;
    const int offRecordMode = 12;
    const int offRecordValid = 16;
    const int offRecordPoint = 20;
    const int offRecordPointZ = 28;
    const int offRecordEntry = 32;
    const int offRecordEntryZ = 40;
    const int offRecordHit = 44;
    const int offRecordHitCopy = 56;
    const int hitCopySize = 232;
    const int recordSize = 512;

    const int offRecordTime = 288;
    const int offRecordPointTime = 292;
    const int offRecordFlags = 296;
    const int offRecordFlag117 = 297;
    const int offRecordValue = 300;
    const int offRecordScore = 304;
    const int offRecordPointExtra = 308;

    const int offStateFlag80 = 80;
    const int offStateFlag120 = 120;
    const int offStateFlag117 = 117;
    const int offPointZ = 8;
    const int offPointExtra = 108;

    if (!ctx || !entry)
        return;

    void* hit = *(void**)((char*)entry + offEntryHit);
    if (!hit)
        return;

    void* point = *(void**)((char*)entry + offEntryPoint);

    // снимок попадания занимает 232 байта и лежит сразу после заголовка
    char record[512];
    memset(record, 0, sizeof(record));
    CopyShotRecord(record + offRecordHitCopy, hit);

    const uint64_t shotTime = part4_recordTime((char*)ctx + offTimeBase, point);

    int targetIndex = -1;
    void* targetData = *(void**)((char*)entry + offEntryTarget);

    if (targetData)
    {
        void* owner = *(void**)((char*)targetData + offOwner);

        if (owner)
            targetIndex = *(int*)((char*)owner + offOwnerIndex);
    }

    *(int*)(record + offRecordSlot) = *(int*)((char*)ctx + offSlot);
    *(int*)(record + offRecordTick) = *(int*)((char*)ctx + offTick);
    *(int*)(record + offRecordTarget) = targetIndex;
    *(int*)(record + offRecordMode) = *(int*)((char*)ctx + offFlag);
    *(uint8_t*)(record + offRecordValid) = 1;

    if (point)
    {
        *(uint64_t*)(record + offRecordPoint) = *(const uint64_t*)point;
        *(int*)(record + offRecordPointZ) = *(const int*)((char*)point + offPointZ);
        *(int*)(record + offRecordPointExtra) = *(const int*)((char*)point + offPointExtra);
    }

    *(uint64_t*)(record + offRecordEntry) = *(const uint64_t*)entry;
    *(int*)(record + offRecordEntryZ) = *(const int*)((char*)entry + offEntryZ);
    *(uint64_t*)(record + offRecordHit) = *(const uint64_t*)hit;

    const int ctxTick = *(int*)((char*)ctx + offTick);
    const int hitTick = *(int*)hit;
    const float hitFrac = *(const float*)((char*)hit + 4);

    *(float*)(record + offRecordTime) =
        part4_timeDelta(ctxTick, 0.f, hitTick, hitFrac);

    *(float*)(record + offRecordPointTime) =
        part4_timeDelta(ctxTick, 0.f, part4_tick(shotTime), part4_frac(shotTime));

    // флаги, которые зависят от режима и состояния на момент выстрела
    *(uint8_t*)(record + offRecordFlags) =
        (*(uint8_t*)((char*)ctx + offStateFlag80)
         && *(uint8_t*)((char*)ctx + offStateFlag120)) ? 1 : 0;
    *(uint8_t*)(record + offRecordFlag117) = *(uint8_t*)((char*)ctx + offStateFlag117);

    *(int*)(record + offRecordValue) = *(const int*)((char*)entry + offEntryValue);
    *(int*)(record + offRecordScore) = *(const int*)((char*)entry + offEntryScore);

    // короткая запись прицеливания: цель, признак и оценка
    struct AimEntry
    {
        int index;
        int extra;
        int value;
    } aimEntry;

    aimEntry.index = targetIndex;
    aimEntry.extra = *(const int*)((char*)entry + offEntryValue);
    aimEntry.value = *(const int*)((char*)entry + offEntryScore);

    if (*(void**)((char*)ctx + offAimEnd) == *(void**)((char*)ctx + offAimCap))
    {
        GrowAimList((char*)ctx + offAimBegin, *(void**)((char*)ctx + offAimEnd), &aimEntry);
    }
    else
    {
        memcpy(*(void**)((char*)ctx + offAimEnd), &aimEntry, sizeof(aimEntry));
        *(char**)((char*)ctx + offAimEnd) += sizeof(aimEntry);
    }

    if (*(void**)((char*)ctx + offRecEnd) == *(void**)((char*)ctx + offRecCap))
    {
        GrowRecordList((char*)ctx + offRecBegin, *(void**)((char*)ctx + offRecEnd), record);
    }
    else
    {
        CopyRecord(*(void**)((char*)ctx + offRecEnd), record);
        *(char**)((char*)ctx + offRecEnd) += recordSize;
    }
}

// ============================================================================
//  лог выстрела
//
//  Пишет одну строку в wl_rage.log: слот, тик, время, позиции и цель.
// ============================================================================

void LogShotEvent(void* ctx, void* entry)
{
    const int offTimeBase = 224;
    const int offSlot = 188;
    const int offTick = 2820;
    const int offTickStep = 2760;
    const int offLatency = 2088;

    const int offOrigin = 152;
    const int offEye = 164;
    const int offPunch = 228;

    const int offEntryTarget = 16;
    const int offEntryPoint = 24;
    const int offEntryDamage = 12;
    const int offPointPerm = 20;
    const int offOwner = 8;
    const int offOwnerIndex = 16;

    if (!g_logReady && !g_moduleReady)
        return;

    if (!ctx || !entry)
        return;

    void* point = *(void**)((char*)entry + offEntryPoint);
    if (!point)
        return;

    const uint64_t shotTime = part4_recordTime((char*)ctx + offTimeBase, point);

    int targetIndex = -1;
    void* targetData = *(void**)((char*)entry + offEntryTarget);

    if (targetData)
    {
        void* owner = *(void**)((char*)targetData + offOwner);

        if (owner)
            targetIndex = *(int*)((char*)owner + offOwnerIndex);
    }

    const Vector origin = *(const Vector*)((char*)ctx + offOrigin);
    const Vector eye = *(const Vector*)((char*)ctx + offEye);
    const Vector punch = *(const Vector*)((char*)ctx + offPunch);
    const Vector aim = *(const Vector*)point;

    const int slot = *(int*)((char*)ctx + offSlot);
    const int tick = *(int*)((char*)ctx + offTick);
    const int step = *(int*)((char*)ctx + offTickStep);
    const float latency = *(const float*)((char*)ctx + offLatency);

    const int shotTick = part4_tick(shotTime);
    const float shotFrac = part4_frac(shotTime);
    const float offset = (float)tick - ((float)shotTick + shotFrac);

    const char* note = (*(const uint8_t*)((char*)point + offPointPerm)) ? " perm" : "";
    const float damage = *(const float*)((char*)entry + offEntryDamage);

    char line[1024];
    snprintf(line, sizeof(line),
             "F %d | tb %d/%d | T %d+%.4f%s | off %.5f s (%.3f t) | "
             "org %.2f %.2f %.2f | eye %.2f %.2f %.2f | punch %.2f %.2f %.2f | "
             "aim %.2f %.2f %.2f | tgt %d dmg %.0f\n",
             slot, tick, step, shotTick, shotFrac, note,
             latency, offset,
             origin.x, origin.y, origin.z,
             eye.x, eye.y, eye.z,
             punch.x, punch.y, punch.z,
             aim.x, aim.y, aim.z,
             targetIndex, damage);

    part4_writeLog(line);
}

// ============================================================================
//  финальная проверка выстрела
//
//  true — выстрел откладывается: запись устарела, не проходит по точности или
//  проигрывает более свежему кандидату.
// ============================================================================

bool FinalShotCheck(void* ctx, void* target, void* record, int flag)
{
    // контекст
    const int offWeaponId = 4448;
    const int offMode = 96;
    const int offDelayShot = 92;
    const int offDelayDamage = 93;
    const int offBlocked = 122;
    const int offTickBase = 132;
    const int offSlot = 188;
    const int offPitch = 76;
    const int offLatency = 8988;
    const int offAimList = 7888;

    // стоп-путь и его временные поля
    const int offStopBase = 8640;
    const int offStopEntry = 8504;
    const int offStopArg = 88;
    const int offStopCopySize = 68;
    const int offStopReady = 5424;
    const int offStopExtra = 5624;
    const int offStopClear = 5592;
    const int offStopListA = 5376;
    const int offStopListAEnd = 5384;
    const int offStopListB = 5400;
    const int offStopListBEnd = 5408;
    const int offStopListC = 5568;
    const int offStopListCEnd = 5576;
    const int offStopResultTick = 5452;
    const int offStopResultTime = 5460;

    // лучшая запись и флаг её готовности
    const int offBestBase = 5344;
    const int offBestFlag = 5544;
    const int offBestData = 16;
    const int offBestScore = 12;
    const int offBestHit = 40;
    const int offBestValue = 68;
    const int offBestTime = 72;
    const int offBestDataValue = 108;

    // проверяемая запись
    const int offTargetData = 16;
    const int offTargetOwner = 8;
    const int offTargetIndex = 16;
    const int offTargetHit = 40;
    const int offTargetValue = 68;
    const int offTargetTime = 72;
    const int offHitTick = 0;

    // запись выстрела
    const int offRecordTime = 288;

    (void)flag;

    ResetWhy();

    if (*(uint16_t*)((char*)ctx + offWeaponId) == 31)
        return false;

    if (!*(uint8_t*)((char*)ctx + offDelayShot)
        && !*(uint8_t*)((char*)ctx + offDelayDamage))
    {
        return false;
    }

    if (*(uint8_t*)((char*)ctx + offBlocked))
        return false;

    if (!target)
        return false;

    void* targetData = *(void**)((char*)target + offTargetData);
    if (!targetData)
        return false;

    void* owner = *(void**)((char*)targetData + offTargetOwner);
    if (!owner)
        return false;

    const int index = *(int*)((char*)owner + offTargetIndex);

    if (index < 0 || index >= (int)(g_targetsEnd - g_targets))
        return false;

    if (g_targets[index].state != 2)
        return false;

    void* hit = *(void**)((char*)target + offTargetHit);
    if (!hit)
        return false;

    const float score = *(float*)((char*)target + offTargetTime);
    const int delta = *(int*)((char*)ctx + offTickBase)
                    - *(int*)((char*)record + offRecordTime)
                    + *(int*)((char*)hit + offHitTick);

    if (score >= 1.f && delta <= 0)
        return false;

    // снимок стоп-пути: дальше работаем уже с ним
    if (record != (char*)ctx + offStopBase)
        memcpy((char*)ctx + offStopBase, record, offStopCopySize);

    *(uint16_t*)((char*)ctx + offStopReady) = 0;
    *(uint64_t*)((char*)ctx + offStopClear) = 0;
    *(uint64_t*)((char*)ctx + offBestBase) = 0;
    *(uint16_t*)((char*)ctx + offStopExtra) = 0;

    part4_resetList((char*)ctx + offStopListA, (char*)ctx + offStopListAEnd);
    part4_resetList((char*)ctx + offStopListB, (char*)ctx + offStopListBEnd);
    part4_resetList((char*)ctx + offStopListC, (char*)ctx + offStopListCEnd);

    StopParams params = MakeStopParams(ctx);
    params.slot = *(int*)((char*)ctx + offSlot);
    params.mode = *(int*)((char*)ctx + offMode);

    SolveStopPath(&params, (char*)ctx + offStopBase, (char*)ctx + offStopEntry, 0,
                  *(int*)((char*)ctx + offStopArg), -1.f);

    BuildStopRequest((char*)ctx + offStopBase, &params, (float*)((char*)ctx + offStopListB));
    StopPass2((SolveContext*)ctx);

    *(uint64_t*)((char*)ctx + offStopResultTick) = *(const uint64_t*)((char*)ctx + offStopBase);
    *(int*)((char*)ctx + offStopResultTime) = *(const int*)((char*)ctx + offStopBase + 8);

    void* best = PickAimEntry((char*)ctx + offBestBase, (char*)ctx + offAimList);
    if (!best)
        return false;

    void* bestData = *(void**)((char*)best + offBestData);
    if (!bestData)
        return false;

    if (*(float*)((char*)bestData + offBestDataValue) > *(float*)((char*)best + offBestScore))
        return false;

    if (*(float*)((char*)ctx + offPitch) > *(float*)((char*)best + offBestValue))
        return false;

    if (!CheckAimRecord(best, (int)*(uint8_t*)((char*)ctx + offBestFlag), 0))
        return false;

    if (RefineShot((SolveContext*)ctx))
        return false;

    // задержка по времени: свежий кандидат лучше старого.
    // В оригинале здесь сравнивались оценки готовности обеих записей, но
    // доступная сигнатура ShotSlotCount(int) даёт одно значение на слот,
    // поэтому сравниваем время самих записей — для них это та же величина.
    if (*(uint8_t*)((char*)ctx + offDelayShot))
    {
        const float time = *(float*)((char*)best + offBestTime);

        if (time > 0.3f && (time - *(float*)((char*)target + offTargetTime)) > 0.1f)
            return true;
    }

    if (!*(uint8_t*)((char*)ctx + offDelayDamage))
        return false;

    const float bestValue = *(float*)((char*)best + offBestValue);
    const float targetValue = *(float*)((char*)target + offTargetValue);

    if (bestValue >= 1.f && targetValue < 1.f)
        return true;

    // допуск на «шум» предсказания
    if (bestValue / fmaxf(targetValue, 0.01f) > 1.04f)
        return true;

    if (delta <= 0)
        return false;

    void* bestHit = *(void**)((char*)best + offBestHit);

    if (!bestHit
        || *(int*)((char*)bestHit + offHitTick) + *(int*)((char*)ctx + offTickBase)
           - *(int*)((char*)ctx + offLatency) >= delta)
    {
        return false;
    }

    return *(float*)((char*)target + offTargetTime) > 0.8f;
}


// ============================================================================
//  добавлено: part5.cpp
// ============================================================================
// ============================================================================
//  чтение памяти
//
//  Все проверки адреса сводятся к одному диапазону: указатель должен лежать
//  в пользовательской части и не попадать в первые 64 КБ.
// ============================================================================

bool Readable(const void* p)
{
    return IsValidAddr((uintptr_t)p);
}

// Одиночный байт из памяти — общий помощник для флагов сущностей.

static uint8_t part5_readByte(const void* p)
{
    if (!Readable(p))
        return 0;

    return *(const uint8_t*)p;
}

int ReadInt(const void* p)
{
    if (!Readable(p))
        return 0;

    return *(const int*)p;
}

float ReadFloat(const void* p)
{
    if (!Readable(p))
        return 0.f;

    return *(const float*)p;
}

uint16_t ReadWord(const void* p)
{
    if (!Readable(p))
        return 0;

    return *(const uint16_t*)p;
}

void* ReadPtr(const void* p)
{
    if (!Readable(p))
        return nullptr;

    return *(void* const*)p;
}

void* ReadVec(void* out, const void* p)
{
    if (!Readable(p))
    {
        if (out)
            *(Vector*)out = Vector();

        return out;
    }

    // без приёмного буфера отдаём адрес источника: вектор читают на месте
    if (!out)
        return const_cast<void*>(p);

    *(Vector*)out = *(const Vector*)p;
    return out;
}

float FastSqrt(float value)
{
    // Отрицательный аргумент даёт NaN — вызывающий код рассчитывает на это
    // в ветке с "быстрым" корнем.
    return sqrtf(value);
}

// ============================================================================
//  плоский список
// ============================================================================

// push_back для списка из трёх указателей (начало / конец / ёмкость).
// Рост как у MSVC: новая ёмкость = ёмкость + половина ёмкости.

static void part5_pushBack(char** list, const void* value, int elemSize)
{
    if (!list || !value || elemSize <= 0)
        return;

    char* begin = list[0];
    char* end   = list[1];
    char* cap   = list[2];

    if (end != cap)
    {
        memcpy(end, value, (size_t)elemSize);
        list[1] = end + elemSize;
        return;
    }

    const size_t count    = (size_t)(end - begin) / (size_t)elemSize;
    const size_t capacity = (size_t)(cap - begin) / (size_t)elemSize;

    size_t grown = capacity + capacity / 2;
    if (grown < count + 1)
        grown = count + 1;

    const size_t maxCount = (size_t)0x7FFFFFFF / (size_t)elemSize;
    if (grown > maxCount)
        grown = maxCount;

    char* memory = (char*)malloc(grown * (size_t)elemSize);
    if (!memory)
        return;

    if (count)
        memcpy(memory, begin, count * (size_t)elemSize);

    memcpy(memory + count * (size_t)elemSize, value, (size_t)elemSize);

    if (begin)
        free(begin);

    list[0] = memory;
    list[1] = memory + (count + 1) * (size_t)elemSize;
    list[2] = memory + grown * (size_t)elemSize;
}

void AppendPos(void* list, void* entry)
{
    const int posSize = (int)sizeof(Vector);    // элемент списка — точка

    if (!entry)
        return;

    part5_pushBack((char**)list, entry, posSize);
}

// ============================================================================
//  снимок углов обзора
//
//  Запись предсказания — 172 байта. Каждое поле заполняется только если в
//  команде выставлен соответствующий флаг, иначе остаётся значение по
//  умолчанию (нули, у скоростей — минус единица).
// ============================================================================

struct part5_ViewSnapshot
{
    Vector  origin;         // 0x00 позиция
    int     tick;           // 0x0C тик
    float   fraction;       // 0x10 дробная часть тика
    uint8_t hasTick;        // 0x14
    char    pad0[3];
    int     tick2;          // 0x18
    float   fraction2;      // 0x1C
    uint8_t hasTick2;       // 0x20
    char    pad1[3];
    int     index;          // 0x24
    uint8_t hasIndex;       // 0x28
    char    pad2[3];
    Vector  velocity;       // 0x2C скорость
    uint8_t hasVelocity;    // 0x38
    char    pad3[3];
    Vector  velocity2;      // 0x3C
    uint8_t hasVelocity2;   // 0x48
    char    pad4[3];
    Vector  velocity3;      // 0x4C
    uint8_t hasVelocity3;   // 0x58
    char    pad5[3];
    int     moveType;       // 0x5C
    uint8_t hasMoveType;    // 0x60
    char    pad6[3];
    int     flags;          // 0x64
    uint8_t hasFlags;       // 0x68
    char    pad7[3];
    Vector  origin2;        // 0x6C
    uint8_t hasOrigin2;     // 0x78
    char    pad8[3];
    Vector  origin3;        // 0x7C
    uint8_t hasOrigin3;     // 0x88
    char    pad9[3];
    Vector  origin4;        // 0x8C
    uint8_t hasOrigin4;     // 0x98
    char    pad10[3];
    Vector  origin5;        // 0x9C
    uint8_t hasOrigin5;     // 0xA8
    char    pad11[3];
};

static_assert(sizeof(part5_ViewSnapshot) == 172, "ViewSnapshot layout");

// Минус единица в поле скорости хранится как сырые биты: значение
// заведомо невалидно и отличает «поля не было» от настоящего нуля.

static void part5_setMinusOne(float& value)
{
    const uint32_t bits = 0xFFFFFFFFu;
    memcpy(&value, &bits, sizeof(value));
}

// Вектор из трёх последовательных dword по смещению 24.

static void part5_readVector(const void* base, Vector& out)
{
    const int offX = 24;
    const int offY = 28;
    const int offZ = 32;

    out.x = ReadFloat((const char*)base + offX);
    out.y = ReadFloat((const char*)base + offY);
    out.z = ReadFloat((const char*)base + offZ);
}

// Собирает запись предсказания из команды: что отмечено флагами, то и
// переносим, остальное остаётся пустым.

static void part5_buildSnapshot(part5_ViewSnapshot& snap, const void* entity)
{
    const int offFlags    = 16;     // флаги полей команды
    const int offOrigin   = 24;     // указатель на позицию
    const int offView      = 32;
    const int offMove      = 40;
    const int offVelocity  = 48;
    const int offVelocity2 = 56;
    const int offOrigin2   = 64;
    const int offOrigin3   = 72;
    const int offOrigin4   = 80;
    const int offOrigin5   = 88;
    const int offTick      = 96;
    const int offTick2     = 104;
    const int offMoveType  = 112;
    const int offFlags2    = 116;

    memset(&snap, 0, sizeof(snap));

    part5_setMinusOne(snap.velocity.x);
    part5_setMinusOne(snap.velocity.y);
    part5_setMinusOne(snap.velocity2.x);
    part5_setMinusOne(snap.velocity2.y);
    part5_setMinusOne(snap.velocity3.x);
    part5_setMinusOne(snap.velocity3.y);

    const char* cmd = (const char*)entity;

    const void* origin = ReadPtr(cmd + offOrigin);
    if (Readable(origin))
        part5_readVector(origin, snap.origin);

    const int flags = ReadInt(cmd + offFlags);

    // тик и его дробная часть: дробь всегда приводится к [0, 1)
    if (flags & 0x200)
    {
        snap.tick = ReadInt(cmd + offTick);

        const float frac = (flags & 0x400) ? ReadFloat(cmd + offTick + 4) : 0.f;
        snap.fraction = frac;

        if (frac < 0.f || frac >= 1.f)
        {
            const float whole = floorf(frac);
            snap.tick     = snap.tick + (int)whole;
            snap.fraction = frac - whole;
        }

        snap.hasTick = 1;
    }

    if (flags & 0x800)
    {
        snap.tick2 = ReadInt(cmd + offTick2);

        const float frac = (flags & 0x1000) ? ReadFloat(cmd + offTick2 + 4) : 0.f;
        snap.fraction2 = frac;

        if (frac < 0.f || frac >= 1.f)
        {
            const float whole = floorf(frac);
            snap.tick2     = snap.tick2 + (int)whole;
            snap.fraction2 = frac - whole;
        }

        snap.hasTick2 = 1;
    }

    // индекс записи
    if (flags & 2)
    {
        const void* view = ReadPtr(cmd + offView);
        if (Readable(view))
        {
            snap.index    = ReadInt((const char*)view + 24);
            snap.hasIndex = 1;
        }
    }

    // три вектора скорости: x/y/z лежат в порядке z, x, y
    if (flags & 4)
    {
        const void* velocity = ReadPtr(cmd + offMove);
        if (Readable(velocity))
        {
            snap.velocity.z = ReadFloat((const char*)velocity + 24);
            snap.velocity.x = ReadFloat((const char*)velocity + 28);
            snap.velocity.y = ReadFloat((const char*)velocity + 32);
            snap.hasVelocity = 1;
        }
    }

    if (flags & 8)
    {
        const void* velocity = ReadPtr(cmd + offVelocity);
        if (Readable(velocity))
        {
            snap.velocity2.z = ReadFloat((const char*)velocity + 24);
            snap.velocity2.x = ReadFloat((const char*)velocity + 28);
            snap.velocity2.y = ReadFloat((const char*)velocity + 32);
            snap.hasVelocity2 = 1;
        }
    }

    if (flags & 0x10)
    {
        const void* velocity = ReadPtr(cmd + offVelocity2);
        if (Readable(velocity))
        {
            snap.velocity3.z = ReadFloat((const char*)velocity + 24);
            snap.velocity3.x = ReadFloat((const char*)velocity + 28);
            snap.velocity3.y = ReadFloat((const char*)velocity + 32);
            snap.hasVelocity3 = 1;
        }
    }

    // тип движения и флаги команды
    if (flags & 0x2000)
    {
        snap.moveType    = ReadInt(cmd + offMoveType);
        snap.hasMoveType = 1;
    }

    if (flags & 0x4000)
    {
        snap.flags    = ReadInt(cmd + offFlags2);
        snap.hasFlags = 1;
    }

    // остальные позиции — обычный порядок x, y, z
    if (flags & 0x20)
    {
        const void* value = ReadPtr(cmd + offOrigin2);
        if (Readable(value))
        {
            part5_readVector(value, snap.origin2);
            snap.hasOrigin2 = 1;
        }
    }

    if (flags & 0x40)
    {
        const void* value = ReadPtr(cmd + offOrigin3);
        if (Readable(value))
        {
            part5_readVector(value, snap.origin3);
            snap.hasOrigin3 = 1;
        }
    }

    if (flags & 0x80)
    {
        const void* value = ReadPtr(cmd + offOrigin4);
        if (Readable(value))
        {
            part5_readVector(value, snap.origin4);
            snap.hasOrigin4 = 1;
        }
    }

    if (flags & 0x100)
    {
        const void* value = ReadPtr(cmd + offOrigin5);
        if (Readable(value))
        {
            part5_readVector(value, snap.origin5);
            snap.hasOrigin5 = 1;
        }
    }
}

// Копирует записи предсказания из истории команд в приёмный список.

void CopyViewAngles(void* dst, const void* src)
{
    const int offLimit   = 0x10;    // предел числа записей
    const int offEntries = 0x18;    // указатель на { счётчик, записи... }
    const int stride     = 0x08;    // записи — указатели, нулевая не используется

    if (!dst || !src)
        return;

    // приёмный список снимков: очищаем, память не освобождаем
    char** snapshots = (char**)dst;
    if (snapshots[0] != snapshots[1])
        snapshots[1] = snapshots[0];

    const void* entries = ReadPtr((const char*)src + offEntries);
    if (!Readable(entries))
        return;

    int count = ReadInt(entries);

    const int limit = ReadInt((const char*)src + offLimit);
    if (count > limit)
        count = limit;

    for (int i = 0; i < count; ++i)
    {
        // нулевой слот массива пропускаем: записи начинаются со следующего
        const void* entity = ReadPtr((const char*)entries + stride * i + stride);
        if (!Readable(entity))
            continue;

        part5_ViewSnapshot snapshot;
        part5_buildSnapshot(snapshot, entity);

        part5_pushBack(snapshots, &snapshot, (int)sizeof(snapshot));
    }
}

// ============================================================================
//  освобождение временного буфера
// ============================================================================

void DestroyTemp(void* request)
{
    if (!request)
        return;

    // буфер описан теми же тремя указателями: начало / конец / ёмкость
    char** temp = (char**)request;
    if (!temp[0])
        return;

    free(temp[0]);

    temp[0] = nullptr;
    temp[1] = nullptr;
    temp[2] = nullptr;
}

// ============================================================================
//  схема
// ============================================================================

struct part5_SchemaEntry
{
    const char* className;
    const char* fieldName;
    int         offset;
};

int GetSchemaOffset(const char* className, const char* fieldName)
{
    const int cacheSize = 64;

    static part5_SchemaEntry cache[cacheSize] = {};
    static int cached = 0;

    // без схемы смещения неизвестны: все обращения к полям отключаются
    if (!className || !fieldName || !g_schemaSystem)
        return 0;

    for (int i = 0; i < cached; ++i)
    {
        if (strcmp(cache[i].className, className) == 0
            && strcmp(cache[i].fieldName, fieldName) == 0)
            return cache[i].offset;
    }

    // резолв дорогой (хеш имени класса и поля + обход типов) — кэшируем
    const int offset = SchemaOffset(className, fieldName);

    if (cached < cacheSize)
    {
        cache[cached].className = className;
        cache[cached].fieldName = fieldName;
        cache[cached].offset    = offset;
        ++cached;
    }

    return offset;
}

// ============================================================================
//  игроки
// ============================================================================

extern void* g_entitySystem;

void* GetPlayerController(int index)
{
    const int maxPlayers = 64;
    const int offList    = 0x08;    // список сущностей: [0] — мир, [1..64] — игроки
    const int stride     = 0x08;

    if (!g_entitySystem || index < 1 || index > maxPlayers)
        return nullptr;

    void** slot = (void**)((char*)g_entitySystem + offList + stride * index);
    if (!Readable(slot))
        return nullptr;

    void* controller = *slot;
    return Readable(controller) ? controller : nullptr;
}

// ============================================================================
//  время и углы
// ============================================================================

// Округление вниз: в оригинале это SSE-инструкция roundss с режимом
// «к меньшему», которой нормализуют угол до одного оборота.

float GetAngleScale(float angle)
{
    return floorf(angle);
}

// Экспонента: в оригинале — libm-функция expf.

float GetTickInterval(float value)
{
    return expf(value);
}

// Разделяет число на целую и дробную части (libm-функция modf).

float GetTimeFrac(float value, float* out)
{
    if (!out)
        return value;

    return std::modf(value, out);
}

uint32_t HashFloat(uint32_t v)
{
    // Оригинал прогоняет значение через тангенс и возвращает биты результата:
    // так получается детерминированное, но "размазанное" число.
    float value;
    memcpy(&value, &v, sizeof(value));

    const float result = tanf(value);

    uint32_t hash;
    memcpy(&hash, &result, sizeof(hash));
    return hash;
}

// ============================================================================
//  обрезка списка
// ============================================================================

void TruncateList(void* begin, void* end)
{
    if (!begin || !end)
        return;

    // вызывающий передаёт поля начала и конца списка: оставляем выделенную
    // память, но список становится пустым
    char** first = (char**)begin;
    char** last  = (char**)end;

    if (!*first)
        return;

    *last = *first;
}

// ============================================================================
//  таблица углов
// ============================================================================

// Детерминированное перемешивание ключа (xorshift).

static uint32_t part5_mixKey(uint32_t key)
{
    key ^= key >> 13;
    key *= 0x85EBCA6Bu;
    key ^= key >> 17;
    return key;
}

void StartLogThread()
{
    const int tableSize = 4096;
    const int seedBase  = 0x56800000;   // база ключей из оригинала

    // таблица строится один раз за сессию
    if (g_logReady)
        return;

    static uint32_t angleTable[tableSize];

    for (int i = 0; i < tableSize; ++i)
        angleTable[i] = part5_mixKey((uint32_t)i + (uint32_t)seedBase);

    // ключи держим упорядоченными: выборка по тику идёт двоичным поиском
    for (int i = 1; i < tableSize; ++i)
    {
        const uint32_t key = angleTable[i];

        int j = i - 1;
        while (j >= 0 && angleTable[j] > key)
        {
            angleTable[j + 1] = angleTable[j];
            --j;
        }

        angleTable[j + 1] = key;
    }

    g_logReady = true;
}

// ============================================================================
//  лимитеры
//
//  В дампе это большой блок анти-аима на use-клавише, опирающийся на десятки
//  глобалов, которых нет в _preamble.h. Восстановлено ядро: проверка
//  состояния, выбор целевого питча по режиму лимитера, ограничение углов и
//  запись их в углы обзора.
// ============================================================================

static float part5_clamp(float value, float low, float high)
{
    if (value < low)
        return low;

    if (value > high)
        return high;

    return value;
}

static float part5_normalizeAngle(float angle)
{
    // приводим к (-180, 180]: вычитаем целое число оборотов
    return angle - roundf(angle / 360.f) * 360.f;
}

void ApplyLimiters(void* cmd)
{
    const int      offButtons   = 0x70;    // CUserCmd::buttons
    const int      offLocalFlags = 0x68;   // LocalData::flags
    const uint64_t useKey       = 0x20;    // IN_USE
    const float    pitchLimit   = 89.f;    // предел питча

    if (!cmd || !Readable(cmd) || !g_rage || !g_rage->valid)
        return;

    if (!g_settings || !g_settings->limiters || !g_rage->limiters)
        return;

    // лимитер работает только на земле: в воздухе углы не подменяем
    if (g_local && (part5_readByte((const char*)g_local + offLocalFlags) & 1))
        return;

    // на use-клавише анти-аим отключается, иначе срывается взаимодействие
    const uint64_t buttons = *(const uint64_t*)((const char*)cmd + offButtons);
    if (buttons & useKey)
        return;

    float aimPitch = 0.f;
    float aimYaw   = 0.f;
    memcpy(&aimPitch, g_aimAngles,     sizeof(float));
    memcpy(&aimYaw,   g_aimAngles + 4, sizeof(float));

    // целевой питч зависит от режима лимитера
    float targetPitch;
    switch (g_settings->limiterExtra)
    {
    case 1:
        targetPitch = pitchLimit;
        break;
    case 2:
        targetPitch = -pitchLimit;
        break;
    case 3:
        targetPitch = 0.f;
        break;
    case 4:
        targetPitch = (float)((g_local ? g_local->tick : 0) % 3 - 1) * pitchLimit;
        break;
    case 5:
        targetPitch = part5_clamp(aimPitch, -pitchLimit, pitchLimit);
        break;
    default:
        targetPitch = aimPitch;
        break;
    }

    // углы обзора: питч в допустимых пределах, рыскание нормализовано
    float* view = (float*)g_viewAngles;
    view[0] = part5_clamp(targetPitch, -pitchLimit, pitchLimit);
    view[1] = part5_normalizeAngle(aimYaw);
    view[2] = 0.f;

    g_rage->silent = 1;

    if (g_local)
    {
        g_rage->lastTick = g_local->tick;
        g_rage->lastTime = g_local->tickTime;
    }
}


// ============================================================================
//  добавлено: part6.cpp
// ============================================================================
// ============================================================================
//  списки выстрелов и целей
// ============================================================================

// Сколько записей в плоском списке выстрелов.

int ShotCount(const ShotList& list)
{
    const size_t entrySize = 72;        // шаг записи выстрела

    if (!list.begin || list.end <= list.begin)
        return 0;

    return (int)((list.end - list.begin) / entrySize);
}

// Сколько точек собрано в слоте прицеливания. Счётчик лежит в под-объекте
// слота (+32), рядом с номером прохода.

int ShotSlotCount(int slot)
{
    const int slotStride = 4104;        // размер одного слота в g_aimSlots
    const int offState   = 32;          // под-объект просчёта слота
    const int offPoints  = 348;         // число собранных точек

    if (slot < 0 || slot >= MaxShotSlots())
        return 0;

    const char* entry = (const char*)&g_aimSlots + slotStride * slot;
    return *(const int*)(entry + offState + offPoints);
}

// Слотов прицеливания ровно три — под них и размечен g_aimSlots.

int MaxShotSlots()
{
    return 3;
}

// Тик приводится к паре «номер тика / доля тика»: доля всегда в [0, 1),
// иначе целая часть уезжает в номер тика.

ShotSlot ShotSlotIndex(int tick)
{
    const float time = g_local ? g_local->tickTime : 0.f;

    ShotSlot slot;

    slot.index = tick;
    slot.frac  = time;

    if (slot.frac >= 0.f && slot.frac < 1.f)
        return slot;

    // кривое время — раскладываем его на целую и дробную части
    float whole = 0.f;
    float frac  = GetTimeFrac(time, &whole);

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            whole -= 1.f;
        else
            frac = 0.f;
    }

    slot.index = tick + (int)whole;
    slot.frac  = frac;
    return slot;
}

// Есть ли в списке запись с выставленным флагом. Флаги лежат в +8 записи.

bool HasFlagInList(const char* begin, const char* end, uint32_t flag)
{
    const size_t   entrySize = 72;
    const int      offFlags  = 8;
    const uint16_t mask      = (uint16_t)flag;

    if (!begin || !end)
        return false;

    for (const char* it = begin; it != end; it += entrySize)
    {
        if ((*(const uint16_t*)(it + offFlags) & mask) != 0)
            return true;
    }

    return false;
}

// Рост списка выстрелов: запись добавляется в конец, ёмкость растёт в полтора
// раза. Список выстрелов в проекте один, поэтому его границы живут в g_shots*.

void GrowShotList(ShotPos** begin, ShotPos* end, const ShotPos* value)
{
    const size_t entrySize = sizeof(ShotPos);
    const int    growthNum = 1;
    const int    growthDen = 2;

    if (!begin || !value)
        return;

    ShotPos* base = *begin;

    const size_t count    = base ? (size_t)(end - base) : 0;
    const size_t capacity = base ? (size_t)(g_shotsCap - base) : 0;

    size_t newCapacity = capacity;

    if (count < capacity)
    {
        // место ещё есть — просто дописываем
        memcpy(base + count, value, entrySize);
    }
    else
    {
        newCapacity = capacity + capacity * growthNum / growthDen;
        if (newCapacity <= count)
            newCapacity = count + 1;

        ShotPos* mem = (ShotPos*)realloc(base, newCapacity * entrySize);
        if (!mem)
            return;

        memcpy(mem + count, value, entrySize);
        base = mem;
    }

    g_shotsBegin = base;
    g_shotsEnd   = base + count + 1;
    g_shotsCap   = base + newCapacity;
}

// Значения по умолчанию для новой записи списка сканирования.

static void part6_initScanEntry(char* entry)
{
    const int entrySize = 172;
    const int offAngleA = 44;           // ссылки на углы: по умолчанию нет
    const int offAngleB = 60;
    const int offAngleC = 76;
    const int offAim    = 100;

    memset(entry, 0, entrySize);

    *(int*)(entry + offAngleA) = -1;
    *(int*)(entry + offAngleB) = -1;
    *(int*)(entry + offAngleC) = -1;
    *(int*)(entry + offAim)    = -1;
}

// Добавляет в список сканирования одну пустую запись (172 байта).

void GrowScanList()
{
    const size_t entrySize = 172;
    const int    growthNum = 1;
    const int    growthDen = 2;

    const size_t count    = (size_t)(g_scanEnd - g_scanBegin) / entrySize;
    const size_t capacity = (size_t)(g_scanCap - g_scanBegin) / entrySize;

    if (count == capacity)
    {
        size_t newCapacity = capacity + capacity * growthNum / growthDen;
        if (newCapacity <= count)
            newCapacity = count + 1;

        char* mem = (char*)realloc(g_scanBegin, newCapacity * entrySize);
        if (!mem)
            return;

        g_scanBegin = mem;
        g_scanEnd   = mem + count * entrySize;
        g_scanCap   = mem + newCapacity * entrySize;
    }

    part6_initScanEntry(g_scanEnd);
    g_scanEnd += entrySize;
}

// ============================================================================
//  цели
// ============================================================================

// Резервирует место под записи целей, сохраняя уже собранные. Записи
// индексируются номером игрока, поэтому ёмкость доводится ровно до запрошенной.

void ReserveTargets(int count)
{
    const size_t entrySize = sizeof(TargetEntry);

    if (count <= 0)
        return;

    const size_t live     = (size_t)(g_targetsEnd - g_targets);
    const size_t capacity = (size_t)(g_targetsCap - g_targets);

    if ((size_t)count <= capacity)
        return;

    char* mem = (char*)realloc((char*)g_targets, (size_t)count * entrySize);
    if (!mem)
        return;

    g_targets    = (TargetEntry*)mem;
    g_targetsEnd = (TargetEntry*)(mem + live * entrySize);
    g_targetsCap = (TargetEntry*)(mem + (size_t)count * entrySize);

    // свежие записи пустые: индекса ещё нет
    for (size_t i = live; i < (size_t)count; ++i)
    {
        TargetEntry* entry = (TargetEntry*)(mem + i * entrySize);

        memset(entry, 0, entrySize);
        entry->index = -1;
    }
}

// Вставка цели в один из трёх лучших слотов. Слоты упорядочены по убыванию
// оценки, поэтому всё, что ниже, сдвигается на одну позицию.

void InsertTarget(int slot, float score, int index)
{
    const int bestCount = 3;

    if (!g_targets || slot < 0 || slot >= bestCount || index < 0)
        return;

    for (int i = bestCount - 1; i > slot; --i)
        g_best[i] = g_best[i - 1];

    g_best[slot] = index;

    // оценка (dot по направлению на цель) живёт в поле fov записи
    g_targets[index].index = index;
    g_targets[index].fov   = score;
}

// ============================================================================
//  углы и разброс
// ============================================================================

// Указатель на углы обзора отдаёт игровой колбэк; второй аргумент — слот.

void* GetViewAngles()
{
    typedef void* (*ViewAnglesFn)(void*, int);

    if (!g_viewEntity)
        return nullptr;

    return reinterpret_cast<ViewAnglesFn>(g_viewEntity)(g_viewTarget, 0);
}

// Текущий угол прицеливания: X — тангаж.

float GetAngleX()
{
    return *(const float*)&g_aimAngles[0];
}

// Текущий угол прицеливания: Y — рыскание.

float GetAngleY()
{
    return *(const float*)&g_aimAngles[4];
}

// Порог по косинусу половины FOV: с ним сравнивается dot направления.

// Косинус полного угла обзора: снапшот берёт field_of_view из настроек,
// переводит градусы в радианы и считает косинус. Деления на два нет.

float GetCurrentSpread()
{
    const float pi         = 3.14159265f;
    const float halfCircle = 180.f;     // градусы в полуокружности

    return cosf(cfg_field_of_view * pi / halfCircle);
}

// Предел разброса при включённых лимитерах — тот же косинус, но от
// limiters.aim_fov.

float GetSpreadLimit()
{
    const float pi         = 3.14159265f;
    const float halfCircle = 180.f;     // градусы в полуокружности

    return cosf(cfg_limiters_aim_fov * pi / halfCircle);
}

// ============================================================================
//  группы оружия
// ============================================================================

// Группа оружия по его id. Группа 0 — общая, её оружие подходит всегда,
// поэтому поиск идёт с последней группы к первой.

int ResolveGroupForWeapon(int* groups, int weaponId)
{
    const int maxGroups       = 64;     // больше групп не поддерживается
    const int groupStride     = 82;     // шаг группы в int'ах (328 байт)
    const int weaponListStart = 17;     // начало списка id оружия
    const int weaponCountPos  = 81;     // количество id в группе

    if (!groups || weaponId <= 0)
        return 0;

    int count = *groups;

    if (count < 1)
        count = 1;
    else if (count > maxGroups)
        count = maxGroups;

    for (int group = count - 1; group >= 1; --group)
    {
        const int* entry = groups + groupStride * group;

        int weapons = entry[weaponCountPos];

        if (weapons < 0)
            weapons = 0;
        else if (weapons > maxGroups)
            weapons = maxGroups;

        for (int i = 0; i < weapons; ++i)
        {
            if (entry[weaponListStart + i] == weaponId)
                return group;
        }
    }

    return 0;
}

// ============================================================================
//  данные цели
// ============================================================================

// Снимок данных цели: шапка записи плюс блок полей из сущности.

void* GetTargetData(void* out, void* target)
{
    const size_t entrySize   = 208;     // размер снимка
    const int    offRecord   = 8;       // ссылка на запись цели
    const int    offSlot     = 32;      // второй указатель записи
    const int    offData     = 48;      // указатель на блок данных цели
    const int    offSource   = 4000;    // начало блока полей
    const int    fieldsCount = 19;      // сколько int'ов переносится
    const int    offFlag     = 4100;    // байт-флаг цели
    const int    offHasData  = 20;      // признак «данные есть»
    const int    offFields   = 24;      // начало перенесённых полей
    const int    offFlagOut  = 16;
    const int    offRecordOut = 0;      // ссылка на запись цели
    const int    offSlotOut   = 8;      // второй указатель записи
    const int    fieldSize   = sizeof(int);  // шаг переносимого поля

    char* dst = (char*)out;
    char* src = (char*)target;

    if (!dst)
        return dst;

    memset(dst, 0, entrySize);

    if (!src)
        return dst;

    *(uint64_t*)(dst + offRecordOut) = *(const uint64_t*)(src + offRecord);
    *(uint64_t*)(dst + offSlotOut)   = *(const uint64_t*)(src + offSlot);

    const char* data = *(const char**)(src + offData);
    *(int*)(dst + offHasData) = (data != nullptr);

    if (!data)
        return dst;

    for (int i = 0; i < fieldsCount; ++i)
        *(int*)(dst + offFields + fieldSize * i) =
            *(const int*)(data + offSource + fieldSize * i);

    *(uint8_t*)(dst + offFlagOut) = *(const uint8_t*)(data + offFlag);
    return dst;
}

// Добавляет снимок цели в список (шаг записи 208 байт).

void AppendTargetData(void* list, void* data)
{
    const size_t entrySize = 208;
    const int    growthNum = 1;
    const int    growthDen = 2;
    const int    offBegin  = 0;         // начало списка
    const int    offEnd    = 8;         // конец списка
    const int    offCap    = 16;        // ёмкость списка

    if (!list || !data)
        return;

    char*  begin = *(char**)((char*)list + offBegin);
    char*  end   = *(char**)((char*)list + offEnd);
    char*  cap   = *(char**)((char*)list + offCap);

    if (end != cap)
    {
        memcpy(end, data, entrySize);
        *(char**)((char*)list + offEnd) = end + entrySize;
        return;
    }

    const size_t count = (size_t)(end - begin) / entrySize;
    size_t newCapacity = (size_t)(cap - begin) / entrySize;

    newCapacity = newCapacity + newCapacity * growthNum / growthDen;
    if (newCapacity <= count)
        newCapacity = count + 1;

    char* mem = (char*)realloc(begin, newCapacity * entrySize);
    if (!mem)
        return;

    memcpy(mem + count * entrySize, data, entrySize);

    *(char**)((char*)list + offBegin) = mem;
    *(char**)((char*)list + offEnd)   = mem + (count + 1) * entrySize;
    *(char**)((char*)list + offCap)   = mem + newCapacity * entrySize;
}

// ============================================================================
//  позиция глаз из объекта предсказания
// ============================================================================

// Позиция глаз: origin из сцены плюс view offset сущности. Если offset
// невменяемый — берём стандартную высоту глаз.

const void* GetExtraTicks(const void* pred)
{
    static Vector eye;
    static int    offViewOffset = 0;

    const int   offSceneNode  = off_m_pGameSceneNode;
    const int   offOrigin     = off_m_vecAbsOrigin;
    const float defaultHeight = 64.f;   // стандартная высота глаз
    const float maxViewHeight = 128.f;  // правдоподобный предел смещения

    eye = Vector();

    const void* entity = pred ? *(const void* const*)pred : nullptr;
    if (!entity)
        return &eye;

    if (!offViewOffset && g_schemaSystem)
        offViewOffset = GetSchemaOffset("C_BaseEntity", "m_vecViewOffset");

    if (offSceneNode && offOrigin)
    {
        const void* node = ReadPtr((const char*)entity + offSceneNode);

        if (node)
        {
            const void* origin = ReadVec(nullptr, (const char*)node + offOrigin);

            if (IsValid(origin))
                eye = *(const Vector*)origin;
        }
    }

    if (offViewOffset)
    {
        const void* view = ReadVec(nullptr, (const char*)entity + offViewOffset);

        if (IsValid(view))
        {
            const Vector offset = *(const Vector*)view;

            // смещение должно быть конечным и правдоподобным
            if (std::isfinite(offset.z) && offset.z > 0.f && offset.z < maxViewHeight)
            {
                eye.x += offset.x;
                eye.y += offset.y;
                eye.z += offset.z;
                return &eye;
            }
        }
    }

    eye.z += defaultHeight;
    return &eye;
}

// ============================================================================
//  просчёт выстрела
// ============================================================================

// Присваивает списку записей (begin/end/cap) диапазон источника.

static void part6_assignEntries(char* vector, const char* source,
                                const char* sourceEnd, size_t entrySize)
{
    const int offEnd = 8;               // конец списка
    const int offCap = 16;              // ёмкость списка

    if (!vector)
        return;

    char* begin = *(char**)vector;
    char* cap   = *(char**)(vector + offCap);

    const size_t count = (source && sourceEnd > source)
                             ? (size_t)(sourceEnd - source) / entrySize
                             : 0;

    const size_t capacity = (cap && cap > begin) ? (size_t)(cap - begin) / entrySize : 0;

    if (count > capacity)
    {
        char* mem = (char*)realloc(begin, count * entrySize);
        if (!mem)
            return;

        begin = mem;

        *(char**)vector        = begin;
        *(char**)(vector + offCap) = begin + count * entrySize;
    }

    if (count)
        memcpy(begin, source, count * entrySize);

    *(char**)(vector + offEnd) = begin + count * entrySize;
}

// Переносит результат просчёта в запись выстрела: углы, тик, кнопки и список
// точек. Всё, что осталось в запросе, копируется как есть.

void ApplySolveResult(void* request, void* shot, void* buttons)
{
    const size_t entrySize = 72;

    // поля запроса
    const int reqRecord  = 8;           // ссылка на запись цели
    const int reqSlot    = 16;          // второй указатель записи
    const int reqButtons = 24;          // кнопки
    const int reqAngles  = 32;          // пара углов
    const int reqAngleX  = 40;          // тангаж
    const int reqAngleY  = 44;          // рыскание
    const int reqTag     = 52;          // метка углов
    const int reqExtra   = 112;         // дополнительный указатель
    const int reqCount   = 120;         // счётчик точек
    const int reqList    = 64;          // начало списка точек
    const int reqListEnd = 72;          // конец списка точек
    const int reqFlagA   = 124;         // флаг записи
    const int reqFlagB   = 125;         // флаг записи

    // поля записи выстрела
    const int shotAngleX = 8;           // тангаж
    const int shotAngleY = 12;          // рыскание
    const int shotTag    = 20;          // метка углов
    const int shotExtra  = 24;          // дополнительный указатель
    const int shotCount  = 32;          // счётчик точек
    const int shotFlagA  = 36;
    const int shotFlagB  = 37;
    const int shotRecord = 40;          // ссылка на запись цели
    const int shotSlot   = 48;          // второй указатель записи
    const int shotList   = 56;          // начало списка точек

    char* req = (char*)request;
    char* dst = (char*)shot;

    if (!req || !dst || !buttons)
        return;

    *(uint64_t*)(dst + shotRecord) = *(const uint64_t*)(req + reqRecord);
    *(uint64_t*)(dst + shotSlot)   = *(const uint64_t*)(req + reqSlot);
    *(uint64_t*)buttons            = *(const uint64_t*)(req + reqButtons);

    *(uint64_t*)dst               = *(const uint64_t*)(req + reqAngles);
    *(int*)(dst + shotAngleX)     = *(const int*)(req + reqAngleX);
    *(uint64_t*)(dst + shotAngleY) = *(const uint64_t*)(req + reqAngleY);
    *(int*)(dst + shotTag)        = *(const int*)(req + reqTag);
    *(uint64_t*)(dst + shotExtra) = *(const uint64_t*)(req + reqExtra);
    *(int*)(dst + shotCount)      = *(const int*)(req + reqCount);

    // список точек запроса переезжает в запись выстрела
    if (dst != req + reqRecord)
        part6_assignEntries(dst + shotList, *(char**)(req + reqList),
                            *(char**)(req + reqListEnd), entrySize);

    *(uint8_t*)(dst + shotFlagA) = *(const uint8_t*)(req + reqFlagA);
    *(uint8_t*)(dst + shotFlagB) = *(const uint8_t*)(req + reqFlagB);
}

// ============================================================================
//  проход по записям выстрела
//
//  Каждая запись несёт кнопку движения и остаток поправки по осям. Проход
//  накапливает поправку, гасит остаток в записях, которые ещё не учтены, и
//  обновляет счётчик/время прохода в блоке запроса.
// ============================================================================

void RunSolvePass(void* out, void* request, int limit, int force)
{
    (void)limit;
    (void)force;

    const size_t entrySize = 72;

    // поля записи выстрела
    const int offButton    = 8;         // кнопка движения
    const int offSign      = 16;        // знак направления
    const int offAngleA    = 36;        // поправка по первой оси
    const int offAngleB    = 40;        // поправка по второй оси
    const int offAngleTick = 44;        // тик поправки
    const int offHasAngles = 48;        // у записи есть поправка углов
    const int offStepX     = 52;        // остаток шага по X
    const int offStepY     = 56;        // остаток шага по Y
    const int offStepTick  = 60;        // тик остатка
    const int offHasStep   = 64;        // у записи есть шаг
    const int offZeroStep  = 65;        // шаг нужно довести до нуля
    const int offProcessed = 66;        // запись уже учтена

    // поля запроса и блока прохода
    const int offAccA      = 32;        // накопленная поправка по первой оси
    const int offAccB      = 36;        // накопленная поправка по второй оси
    const int offListBegin = 64;        // начало списка записей
    const int offListEnd   = 72;        // конец списка записей
    const int offSkipSolve = 126;       // просчёт уже выполнен
    const int offMoveFlags = 376;       // активные кнопки движения
    const int offMoveDelta = 452;       // пара float: вперёд/в сторону
    const int offMoveSide  = 456;       // вторая компонента пары
    const int offPassCount = 288;       // счётчик проходов
    const int offPassIndex = 348;
    const int offTickLen   = 352;
    const int offPassTime  = 356;
    const int offPassFlags = 360;

    const uint64_t forwardMask = 0x18;  // вперёд/назад
    const uint64_t lateralMask = 0x600; // влево/вправо
    const float    moveLimit   = 10.f;  // разумный предел поправки
    const float    tickLength  = 0.015625f;
    const float    stepEpsilon = 0.001f; // минимальный ненулевой остаток

    const uint64_t moveForward = 8;
    const uint64_t moveBack    = 16;
    const uint64_t moveLeft    = 512;
    const uint64_t moveRight   = 1024;

    if (!out || !request)
        return;

    char* block = (char*)out;
    char* req   = (char*)request;

    if (!*(const uint8_t*)(req + offSkipSolve))
    {
        // стартовые поправки берём из текущих кнопок движения
        const uint64_t moveFlags = *(const uint64_t*)(block + offMoveFlags);

        float forward = *(const float*)(block + offMoveDelta);
        float lateral = *(const float*)(block + offMoveSide);

        if ((moveFlags & forwardMask) == 0 || !std::isfinite(forward) || fabsf(forward) >= moveLimit)
            forward = 0.f;

        if ((moveFlags & lateralMask) == 0 || !std::isfinite(lateral) || fabsf(lateral) >= moveLimit)
            lateral = 0.f;

        // накопленные поправки углов продолжают то, что уже лежит в запросе
        float angleA = *(const float*)(req + offAccA);
        float angleB = *(const float*)(req + offAccB);

        char* begin = *(char**)(req + offListBegin);
        char* end   = *(char**)(req + offListEnd);

        for (char* entry = begin; entry != end; entry += entrySize)
        {
            const uint64_t button = *(const uint64_t*)(entry + offButton);
            const bool     sign   = *(const uint8_t*)(entry + offSign) != 0;

            // кнопка задаёт знак накопления по своей оси
            switch (button)
            {
            case moveForward: forward += sign ?  1.f : -1.f; break;
            case moveBack:    forward += sign ? -1.f :  1.f; break;
            case moveLeft:    lateral += sign ?  1.f : -1.f; break;
            case moveRight:   lateral += sign ? -1.f :  1.f; break;
            default:          break;
            }

            if (*(const uint8_t*)(entry + offProcessed))
            {
                // запись уже учтена: её остатки возвращаются в накопление
                if (button == 0 && *(const uint8_t*)(entry + offHasStep))
                {
                    forward += *(const float*)(entry + offStepX);
                    lateral += *(const float*)(entry + offStepY);
                }

                if (*(const uint8_t*)(entry + offHasAngles))
                {
                    angleA += *(const float*)(entry + offAngleA);
                    angleB += *(const float*)(entry + offAngleB);
                }

                continue;
            }

            *(uint8_t*)(entry + offProcessed) = 1;

            bool done = false;

            if (button == 0)
            {
                if (*(const uint8_t*)(entry + offHasStep))
                {
                    // остаток шага: сколько ещё нужно сдвинуть, чтобы попасть в точку
                    float restX = *(const float*)(entry + offStepX) - forward;
                    float restY = *(const float*)(entry + offStepY) - lateral;

                    if (restX == 0.f)
                        restX = 0.f;
                    else
                        forward += restX;

                    if (restY == 0.f)
                        restY = 0.f;
                    else
                        lateral += restY;

                    *(float*)(entry + offStepX) = restX;
                    *(float*)(entry + offStepY) = restY;
                    *(int*)(entry + offStepTick) = 0;

                    done = (restX == 0.f && restY == 0.f);
                }
                else
                {
                    done = true;
                }
            }

            if (*(const uint8_t*)(entry + offHasAngles))
            {
                // поправка по осям гасится накопленной
                float restA = *(const float*)(entry + offAngleA) - angleA;

                if (restA == 0.f)
                    restA = 0.f;
                else
                    angleA += restA;

                *(float*)(entry + offAngleA) = restA;

                float restB = *(const float*)(entry + offAngleB) - angleB;
                *(int*)(entry + offAngleTick) = 0;

                if (restB == 0.f)
                {
                    *(float*)(entry + offAngleB) = 0.f;

                    // нулевой остаток подменяем минимальным, иначе шаг «залипает»
                    if (restA == 0.f && done && *(const uint8_t*)(entry + offZeroStep))
                    {
                        *(float*)(entry + offAngleA) = stepEpsilon;
                        angleA += stepEpsilon;
                    }
                }
                else
                {
                    angleB += restB;
                    *(float*)(entry + offAngleB) = restB;
                }

                continue;
            }

            if (done && *(const uint8_t*)(entry + offZeroStep))
            {
                *(uint64_t*)(entry + offAngleB) = 0;    // +40..47
                *(uint8_t*)(entry + offHasAngles) = 1;
                *(float*)(entry + offAngleA) = stepEpsilon;
                angleA += stepEpsilon;
            }
        }
    }

    // отмечаем проход в блоке запроса
    const int pass = ++*(int*)(block + offPassCount);

    *(int*)(block + offPassIndex)  = pass;
    *(float*)(block + offTickLen)  = tickLength;
    *(float*)(block + offPassTime) = (float)pass * tickLength;
    *(int*)(block + offPassFlags)  = 0;
}

// Объединение результатов просчёта: тот же проход, но без ограничений.

void MergeSolve(void* out, void* request)
{
    RunSolvePass(out, request, 0, 0);
}


// ============================================================================
//  добавлено: part7.cpp
// ============================================================================
// ============================================================================
//  конвейер точек прицеливания
//
//  Список прицеливания (list) — заголовок внутри SolveContext. Он хранит:
//    +0    идентификатор команды (predId)
//    +16   снимок контекста
//    +24   тик
//    +32   записи целей (шаг 208)
//    +56   точки (шаг 96)
//    +80   признак «список собран»
//    +84   углы точки
//    +108  запись команды (id + тик)
//    +204  количество точек
//    +224  результаты отбора (шаг 88)
// ============================================================================

// ---------------------------------------------------------------------------
//  раскладка записи цели (208 байт)
// ---------------------------------------------------------------------------
static const int kEntryData        = 8;    // владелец цели (объект с индексом +16)
static const int kEntryTargetData  = 16;   // живой блок данных цели
static const int kEntryPointData   = 24;   // точка (96-байтовая запись)
static const int kEntrySlot        = 32;   // слот прицеливания
static const int kEntryHitData     = 40;   // буфер попадания (статический пул)
static const int kEntryTimeFrac    = 48;
static const int kEntryIndex       = 52;
static const int kEntryTeam        = 56;
static const int kEntryDist        = 60;
static const int kEntryFlags       = 64;
static const int kEntryHitchance   = 68;
static const int kEntryDelayFrac   = 72;
static const int kEntryTick        = 76;
static const int kEntryHit         = 80;
static const int kEntryKnife       = 81;
static const int kEntryWalls       = 82;
static const int kEntryPrepared    = 83;
static const int kEntryAngles      = 84;
static const int kEntryAngleTag    = 92;
static const int kEntryAngleMax    = 96;
static const int kEntryTargetTick  = 100;
static const int kEntryHitboxMask  = 104;
static const int kEntryDamage      = 108;
static const int kEntrySpreadSteps = 112;
static const int kEntryOrigin      = 116;
static const int kEntryList        = 200;
static const int kEntrySize        = 208;

// ---------------------------------------------------------------------------
//  раскладка результата отбора (88 байт)
// ---------------------------------------------------------------------------
static const int kResultOrigin     = 0;
static const int kResultDamage     = 12;
static const int kResultTargetData = 16;
static const int kResultPointData  = 24;
static const int kResultOwner      = 32;
static const int kResultHitData    = 40;
static const int kResultAngles     = 48;
static const int kResultTimeFrac   = 56;
static const int kResultIndex      = 60;
static const int kResultFlags      = 64;
static const int kResultHitchance  = 68;
static const int kResultDelayFrac  = 72;
static const int kResultTick       = 76;
static const int kResultHit        = 80;
static const int kResultKnife      = 81;
static const int kResultWalls      = 82;
static const int kResultSize       = 88;
static const int kResultMax        = 32;

// ---------------------------------------------------------------------------
//  раскладка кандидата (56 байт)
// ---------------------------------------------------------------------------
static const int kCandHitbox     = 0;
static const int kCandDamage     = 4;
static const int kCandHitchance  = 8;
static const int kCandDistance   = 12;
static const int kCandFlags      = 16;
static const int kCandIndex      = 20;
static const int kCandPoint      = 24;
static const int kCandDir        = 36;
static const int kCandHit        = 48;
static const int kCandWalls      = 49;
static const int kCandGroup      = 50;
static const int kCandTick       = 52;
static const int kCandSize       = 56;

// ---------------------------------------------------------------------------
//  раскладка буфера попадания (160 байт)
// ---------------------------------------------------------------------------
static const int kHitTick       = 0;
static const int kHitFrac       = 4;
static const int kHitTag        = 8;
static const int kHitHasAngles  = 12;
static const int kHitGroup      = 13;
static const int kHitAngles     = 16;
static const int kHitPoint      = 24;
static const int kHitDistance   = 36;
static const int kHitDamage     = 40;
static const int kHitbox        = 44;
static const int kHitWalls      = 48;
static const int kHitValid      = 52;
static const int kHitChance     = 56;
static const int kHitIndex      = 60;
static const int kHitAge        = 64;
static const int kHitDamageAlt  = 108;
static const int kHitAnglesAlt  = 120;
static const int kHitResolved   = 124;
static const int kHitSize       = 160;

// ---------------------------------------------------------------------------
//  раскладка состояния цели (96 байт), которое строит BuildTargetState
// ---------------------------------------------------------------------------
static const int kStateData      = 0;
static const int kStateLive      = 8;
static const int kStateIndex     = 16;
static const int kStateTeam      = 20;
static const int kStateOrigin    = 24;
static const int kStateDistance  = 36;
static const int kStateDamage    = 40;
static const int kStateTick      = 44;
static const int kStateFov       = 48;
static const int kStateValid     = 52;
static const int kStateKnife     = 53;
static const int kStateFlags     = 54;
static const int kStateMask      = 56;
static const int kStateHitchance = 60;
static const int kStateAngleScale = 64;
static const int kStateSpread    = 68;
static const int kStateSize      = 96;

// ---------------------------------------------------------------------------
//  раскладка точки (96 байт)
// ---------------------------------------------------------------------------
static const int kPointPos      = 0;
static const int kPointAngleMax = 12;
static const int kPointRadius   = 16;
static const int kPointTarget   = 24;
static const int kPointAngles   = 80;
static const int kPointTag      = 88;
static const int kPointSize     = 96;

// масштабы, используемые при разборе точек
static const float kLateralScale   = 0.25f;
static const float kSecondVariant  = 0.85f;
static const float kAngleHashScale = 1000.f;
static const float kTickToTime     = 0.015625f;   // 1/64 секунды

// ---------------------------------------------------------------------------
//  смещение хитбокса относительно уже выбранной точки
// ---------------------------------------------------------------------------

static void part7_hitboxOffset(int hitbox, float radius, float* outX, float* outZ)
{
    const float side = radius * kLateralScale;

    switch (hitbox)
    {
    case 0:  // голова
        *outX = 0.f;
        *outZ = radius * 0.35f;
        break;
    case 1:  // грудь
        *outX = 0.f;
        *outZ = radius * 0.15f;
        break;
    case 2:  // живот
        *outX = 0.f;
        *outZ = 0.f;
        break;
    case 3:  // таз
        *outX = side * 0.5f;
        *outZ = -radius * 0.20f;
        break;
    default: // ноги
        *outX = side;
        *outZ = -radius * 0.40f;
        break;
    }
}

// множитель урона по хитбоксу

static float part7_hitboxScale(int hitbox)
{
    switch (hitbox)
    {
    case 0:  return 4.0f;
    case 1:  return 1.0f;
    case 2:  return 1.25f;
    case 3:  return 0.75f;
    default: return 0.60f;
    }
}

// ---------------------------------------------------------------------------
//  Пул буферов попадания.
//
//  Буфер нужен только на время прохода и привязан к номеру записи в списке,
//  поэтому храним его статически, а не выделяем на каждую запись.
// ---------------------------------------------------------------------------

static char* part7_hitStorage(int slot)
{
    const int hitMax = 64;

    static char storage[hitMax][kHitSize];

    if (slot < 0)
        slot = 0;

    if (slot >= hitMax)
        slot = hitMax - 1;

    char* hit = storage[slot];
    memset(hit, 0, kHitSize);

    return hit;
}

// ---------------------------------------------------------------------------
//  Хранилище результатов отбора для конкретного списка.
// ---------------------------------------------------------------------------

static char* part7_resultStorage(char* list)
{
    const int listMax = 4;

    static char* keys[listMax] = {};
    static char* pool[listMax] = {};
    static int   used = 0;

    for (int i = 0; i < used; ++i)
    {
        if (keys[i] == list)
            return pool[i];
    }

    if (used >= listMax)
        return nullptr;

    char* buffer = (char*)malloc((size_t)kResultSize * (size_t)kResultMax);

    if (!buffer)
        return nullptr;

    keys[used] = list;
    pool[used] = buffer;
    ++used;

    return buffer;
}

// резервируем место под результаты отбора, если его ещё нет

static void part7_ensureResults(char* list)
{
    const int offBegin = 224;
    const int offEnd   = 232;
    const int offCap   = 240;

    char* begin = *(char**)(list + offBegin);
    char* cap   = *(char**)(list + offCap);

    // уже выделено и выглядит правдоподобно
    if (begin && cap && cap > begin
        && (size_t)(cap - begin) == (size_t)kResultSize * (size_t)kResultMax)
    {
        return;
    }

    char* buffer = part7_resultStorage(list);

    if (!buffer)
        return;

    *(char**)(list + offBegin) = buffer;
    *(char**)(list + offEnd)   = buffer;
    *(char**)(list + offCap)   = buffer + (size_t)kResultSize * (size_t)kResultMax;
}

// снимок записи цели в компактный результат отбора

static void part7_buildResult(char* result, char* entry)
{
    memset(result, 0, kResultSize);

    *(Vector*)(result + kResultOrigin)   = *(const Vector*)(entry + kEntryOrigin);
    *(float*)(result + kResultDamage)    = *(const float*)(entry + kEntryDamage);
    *(void**)(result + kResultTargetData) = entry;
    *(void**)(result + kResultPointData) = *(void**)(entry + kEntryPointData);
    *(void**)(result + kResultOwner)     = *(void**)(entry + kEntryData);
    *(void**)(result + kResultHitData)   = *(void**)(entry + kEntryHitData);
    *(uint64_t*)(result + kResultAngles) = *(const uint64_t*)(entry + kEntryAngles);
    *(float*)(result + kResultTimeFrac)  = *(const float*)(entry + kEntryTimeFrac);
    *(int*)(result + kResultIndex)       = *(const int*)(entry + kEntryIndex);
    *(int*)(result + kResultFlags)       = *(const int*)(entry + kEntryFlags);
    *(float*)(result + kResultHitchance) = *(const float*)(entry + kEntryHitchance);
    *(float*)(result + kResultDelayFrac) = *(const float*)(entry + kEntryDelayFrac);
    *(int*)(result + kResultTick)        = *(const int*)(entry + kEntryTick);
    *(uint8_t*)(result + kResultHit)     = *(const uint8_t*)(entry + kEntryHit);
    *(uint8_t*)(result + kResultKnife)   = *(const uint8_t*)(entry + kEntryKnife);
    *(uint8_t*)(result + kResultWalls)   = *(const uint8_t*)(entry + kEntryWalls);
}

// регистрирует попадание в списке результатов отбора

static void part7_registerResult(char* entry)
{
    const int offBegin = 224;
    const int offEnd   = 232;
    const int offCap   = 240;

    char* list = *(char**)(entry + kEntryList);

    if (!list)
        return;

    part7_ensureResults(list);

    char* begin = *(char**)(list + offBegin);
    char* end   = *(char**)(list + offEnd);
    char* cap   = *(char**)(list + offCap);

    if (!begin || !end || !cap || end < begin || cap < end)
        return;

    // запись уже публиковалась — обновляем её снимок на месте
    for (char* it = begin; it + kResultSize <= end; it += kResultSize)
    {
        if (*(void**)(it + kResultTargetData) == entry)
        {
            part7_buildResult(it, entry);
            return;
        }
    }

    if ((size_t)(cap - end) < (size_t)kResultSize)
        return;

    part7_buildResult(end, entry);
    *(char**)(list + offEnd) = end + kResultSize;
}

// сравнение точек: сначала ближе к локальному игроку, затем меньше угол

static bool part7_pointLess(const char* lhs, const char* rhs, const Vector& origin)
{
    const Vector left  = *(const Vector*)(lhs + kPointPos) - origin;
    const Vector right = *(const Vector*)(rhs + kPointPos) - origin;

    const float leftLen  = left.LengthSqr();
    const float rightLen = right.LengthSqr();

    if (leftLen != rightLen)
        return leftLen < rightLen;

    return *(const float*)(lhs + kPointAngleMax) < *(const float*)(rhs + kPointAngleMax);
}

// ---------------------------------------------------------------------------
//  Копирование векторной части списка: позиция, направление, флаги.
// ---------------------------------------------------------------------------

void PackAimVectors(void* ctx, void* list)
{
    const int offCtxOrigin   = 0;
    const int offCtxDir      = 152;
    const int offCtxMode     = 96;
    const int offCtxFlags    = 4488;
    const int offCtxWeaponId = 4448;

    const int offListOrigin  = 136;
    const int offListDir     = 148;
    const int offListFlags   = 160;
    const int offListCount   = 168;
    const int offListPacked  = 176;
    const int offListKnife   = 177;
    const int offPointsBegin = 56;
    const int offPointsEnd   = 64;

    char* c = (char*)ctx;
    char* l = (char*)list;

    if (!c || !l)
        return;

    memcpy(l + offListOrigin, c + offCtxOrigin, sizeof(Vector));
    memcpy(l + offListDir, c + offCtxDir, sizeof(Vector));

    *(uint64_t*)(l + offListFlags) = *(const uint64_t*)(c + offCtxFlags);

    const char* pointsBegin = *(char**)(l + offPointsBegin);
    const char* pointsEnd   = *(char**)(l + offPointsEnd);

    int count = 0;

    if (pointsBegin && pointsEnd && pointsEnd > pointsBegin)
        count = (int)((pointsEnd - pointsBegin) / kPointSize);

    *(int*)(l + offListCount) = count;

    // нож: режим 2 и оружие 64
    const bool knife = *(const int*)(c + offCtxMode) == 2
                    && *(const uint16_t*)(c + offCtxWeaponId) == 64;

    *(uint8_t*)(l + offListKnife)  = knife ? 1 : 0;
    *(uint8_t*)(l + offListPacked) = 1;
}

// ---------------------------------------------------------------------------
//  Регистрация собранной записи в списке.
// ---------------------------------------------------------------------------

void RegisterAimRecord(void* ctx, void* list, uint64_t id, int flag)
{
    const int offCtxTick    = 2820;
    const int offCtxId      = 2800;
    const int offListValid  = 80;
    const int offListRecId  = 108;
    const int offListRecTick = 116;
    const int recentMax     = 8;

    static void* recent[recentMax] = {};
    static int   recentCount = 0;
    static int   recentNext  = 0;

    char* c = (char*)ctx;
    char* l = (char*)list;

    if (!l)
        return;

    // запись всегда относится к текущей команде
    if (c)
    {
        const uint64_t current = *(const uint64_t*)(c + offCtxId);

        if (!id)
            id = current;
    }

    *(uint64_t*)(l + offListRecId) = id;

    if (c)
        *(int*)(l + offListRecTick) = *(const int*)(c + offCtxTick);

    if (flag)
        *(uint8_t*)(l + offListValid) = 1;

    // одна и та же запись не должна регистрироваться дважды за проход
    for (int i = 0; i < recentCount; ++i)
    {
        if (recent[i] == l)
            return;
    }

    recent[recentNext] = l;
    recentNext = (recentNext + 1) % recentMax;

    if (recentCount < recentMax)
        ++recentCount;

    g_shotDirty = true;
}

// ---------------------------------------------------------------------------
//  Состояние слота на текущем тике: окно тиков, остаток попаданий, флаги.
// ---------------------------------------------------------------------------

void ComputeSlotState(void* request, void* slot, void* inner, int tick, void* extra)
{
    const int offReqTick      = 8;
    const int offReqExtra     = 12;
    const int offReqSlotTick  = 16;
    const int offReqHitCount  = 20;
    const int offReqBudget    = 24;
    const int offReqFlags     = 28;
    const int offReqDamage    = 32;
    const int offReqHitchance = 36;

    const int offSlotTick    = 16;
    const int offSlotExtra   = 20;
    const int offSlotLimitLo = 4076;
    const int offSlotLimitHi = 4080;
    const int offSlotStamp   = 4084;
    const int offSlotHits    = 4088;
    const int offSlotLive    = 4097;
    const int offSlotFresh   = 4098;

    const int offHistTick  = 0;
    const int offHistCount = 4;

    char* q = (char*)request;
    char* s = (char*)slot;
    char* in = (char*)inner;
    char* hist = (char*)extra;

    if (!s)
        return;

    const int slotTick = q ? *(const int*)(q + offReqSlotTick)
                           : *(const int*)(s + offSlotTick);
    const int slotExtra = q ? *(const int*)(q + offReqExtra)
                            : *(const int*)(s + offSlotExtra);

    const int window = g_settings ? g_settings->backtrack : 0;
    const int age = tick - slotTick;

    // окно валидности слота: от самой старой до самой свежей записи
    int oldest = *(const int*)(s + offSlotLimitLo);
    int newest = *(const int*)(s + offSlotLimitHi);

    if (!oldest || slotTick < oldest)
        oldest = slotTick;

    if (!newest || slotTick > newest)
        newest = slotTick;

    *(int*)(s + offSlotLimitLo) = oldest;
    *(int*)(s + offSlotLimitHi) = newest;
    *(int*)(s + offSlotStamp)   = tick;

    // каждый проход расходует одно сохранённое попадание
    int hits = *(const int*)(s + offSlotHits);

    if (hits < 0)
        hits = 0;

    if (hits > 0)
        --hits;

    *(int*)(s + offSlotHits) = hits;

    const bool live  = (window <= 0) ? (age >= 0) : (age >= -window && age <= window);
    const bool fresh = (slotExtra != 0) || (hits > 0);

    *(uint8_t*)(s + offSlotLive)  = live ? 1 : 0;
    *(uint8_t*)(s + offSlotFresh) = fresh ? 1 : 0;

    // данные цели: тик внутри блока не должен опережать текущий
    int innerTick = 0;

    if (in && IsValid((const char*)in + 12) && Readable((const char*)in + 12))
        innerTick = ReadInt((const char*)in + 12);

    if (q)
    {
        *(int*)(q + offReqTick)     = tick;
        *(int*)(q + offReqHitCount) = hits;
        *(int*)(q + offReqBudget)   = g_settings ? g_settings->maxWalls : 0;
        *(int*)(q + offReqFlags)    = (live ? 1 : 0) | (fresh ? 2 : 0)
                                    | ((innerTick <= tick) ? 4 : 0);
        *(int*)(q + offReqDamage)   = 0;
        *(float*)(q + offReqHitchance) = g_settings ? g_settings->hitchance : 0.f;
    }

    // история слота: тик и число прогонов
    if (hist)
    {
        *(int*)(hist + offHistTick) = tick;

        int runs = *(const int*)(hist + offHistCount);

        if (runs < 0)
            runs = 0;

        *(int*)(hist + offHistCount) = runs + 1;
    }
}

// ---------------------------------------------------------------------------
//  Подготовка записи цели к обработке.
// ---------------------------------------------------------------------------

bool PrepareAimEntry(void* base, void* entry, void* extra)
{
    (void)extra;

    const int offListPointsBegin = 56;
    const int offListPointsEnd   = 64;
    const int offListTick        = 24;

    const int offTargetSlot = 48;

    char* b = (char*)base;
    char* e = (char*)entry;

    if (!b || !e)
        return false;

    *(uint8_t*)(e + kEntryPrepared) = 0;

    // владелец цели — объект, у которого +16 хранит индекс в списке целей
    void* owner = *(void**)(e + kEntryData);

    if (!owner || !IsValid((const char*)owner + 16))
        return false;

    const int index = *(const int*)((char*)owner + 16);
    const int targetCount = (int)(g_targetsEnd - g_targets);

    if (index < 0 || index >= targetCount)
        return false;

    // цель должна быть живой
    if (g_targets[index].state != 2)
        return false;

    // точка, по которой будет считаться запись
    char* pointsBegin = *(char**)(b + offListPointsBegin);
    char* pointsEnd   = *(char**)(b + offListPointsEnd);

    if (!pointsBegin || !pointsEnd || pointsEnd <= pointsBegin)
        return false;

    char* point = nullptr;

    for (char* it = pointsBegin; it + kPointSize <= pointsEnd; it += kPointSize)
    {
        void* pointTarget = *(void**)(it + kPointTarget);

        if (pointTarget == entry || pointTarget == owner)
        {
            point = it;
            break;
        }
    }

    if (!point)
        point = pointsBegin;

    *(void**)(e + kEntryPointData) = point;
    *(void**)(e + kEntryList)      = b;

    // позиция глаз: из списка (её пишет PackAimVectors), иначе из локального игрока
    {
        const int offListOrigin = 136;

        Vector eye = *(const Vector*)(b + offListOrigin);

        if (eye.LengthSqr() == 0.f && g_local)
            eye = g_local->origin + g_local->viewOffset;

        *(Vector*)(e + kEntryOrigin) = eye;
    }

    // живой блок данных цели (если владелец на него ссылается)
    void* live = ReadPtrGuarded((char*)owner + 8);

    *(void**)(e + kEntryTargetData) = live;

    // параметры цели
    *(int*)(e + kEntryIndex)      = index;
    *(int*)(e + kEntryTeam)       = g_targets[index].team;
    *(float*)(e + kEntryDist)     = g_targets[index].dist;

    void* slot = *(void**)((char*)&g_targets[index] + offTargetSlot);
    *(void**)(e + kEntrySlot) = slot;

    // буфер попадания привязан к номеру записи в списке
    int slotIndex = 0;
    {
        char* entriesBegin = *(char**)(b + 32);
        const ptrdiff_t offset = (char*)entry - entriesBegin;

        if (entriesBegin && offset >= 0)
            slotIndex = (int)(offset / kEntrySize);
    }

    *(void**)(e + kEntryHitData) = part7_hitStorage(slotIndex);

    // порог хитшанса и урон из настроек
    const float chance = g_settings ? g_settings->hitchance : 0.f;
    *(float*)(e + kEntryHitchance) = chance;

    float damage = 0.f;

    if (live && IsValid((const char*)live + kEntryDamage)
        && Readable((const char*)live + kEntryDamage))
    {
        damage = ReadFloat((const char*)live + kEntryDamage);
    }
    else if (g_settings)
    {
        damage = g_settings->minDamage;
    }

    *(float*)(e + kEntryDamage) = damage;

    // точка задаёт максимальное отклонение и тик
    *(float*)(e + kEntryAngleMax)   = *(const float*)(point + kPointAngleMax);
    *(int*)(e + kEntryTargetTick)   = *(const int*)(point + kPointTag);
    *(int*)(e + kEntryHitboxMask)   = g_settings ? g_settings->hitboxMask : 0;
    *(int*)(e + kEntrySpreadSteps)  = g_settings ? g_settings->refineStep : 0;

    *(int*)(e + kEntryTick) = *(const int*)(b + offListTick);

    if (!*(int*)(e + kEntryTick))
        *(int*)(e + kEntryTick) = *(const int*)(point + kPointTag);

    // попаданий ещё нет
    *(uint8_t*)(e + kEntryHit)   = 0;
    *(uint8_t*)(e + kEntryWalls) = 0;
    *(float*)(e + kEntryDelayFrac) = 0.f;
    *(uint64_t*)(e + kEntryAngles) = 0;
    *(int*)(e + kEntryAngleTag)    = 0;

    // нож
    const bool knife = g_settings && g_settings->group == 2
                    && g_local && g_local->weaponId == 64;

    *(uint8_t*)(e + kEntryKnife) = knife ? 1 : 0;

    // место под результаты отбора готовим заранее
    part7_ensureResults(b);

    *(uint8_t*)(e + kEntryPrepared) = 1;

    return true;
}

// ---------------------------------------------------------------------------
//  Состояние цели для прохода по точкам.
// ---------------------------------------------------------------------------

void BuildTargetState(void* out, void* data)
{
    const int offOwnerIndex = 16;
    const int offOwnerData  = 8;
    const int offLiveDamage = 108;
    const int offLiveTick   = 4044;

    char* s = (char*)out;

    if (!s)
        return;

    memset(s, 0, kStateSize);

    if (!data)
        return;

    *(void**)(s + kStateData) = data;

    // позиция глаз — от неё считаются направления кандидатов
    if (g_local)
        *(Vector*)(s + kStateOrigin) = g_local->origin + g_local->viewOffset;

    const int index = IsValid((const char*)data + offOwnerIndex)
                          ? *(const int*)((char*)data + offOwnerIndex)
                          : -1;

    *(int*)(s + kStateIndex) = index;

    // живой блок данных цели
    void* live = ReadPtrGuarded((char*)data + offOwnerData);

    *(void**)(s + kStateLive) = live;

    const int targetCount = (int)(g_targetsEnd - g_targets);

    if (index >= 0 && index < targetCount)
    {
        const TargetEntry& target = g_targets[index];

        *(int*)(s + kStateTeam)     = target.team;
        *(float*)(s + kStateDistance) = target.dist;
        *(float*)(s + kStateFov)    = target.fov;
        *(uint8_t*)(s + kStateValid) = 1;
    }

    if (live && IsValid((const char*)live + offLiveDamage)
        && Readable((const char*)live + offLiveDamage))
    {
        *(float*)(s + kStateDamage) = ReadFloat((const char*)live + offLiveDamage);
    }

    if (live && IsValid((const char*)live + offLiveTick)
        && Readable((const char*)live + offLiveTick))
    {
        *(int*)(s + kStateTick) = ReadInt((const char*)live + offLiveTick);
    }

    if (g_settings)
    {
        *(uint32_t*)(s + kStateMask)     = (uint32_t)g_settings->hitboxMask;
        *(float*)(s + kStateHitchance)   = g_settings->hitchance;
        *(int*)(s + kStateSpread)        = g_settings->refineStep;
    }

    *(float*)(s + kStateAngleScale) = GetAngleScale(1.f);

    const bool knife = g_settings && g_settings->group == 2
                    && g_local && g_local->weaponId == 64;

    *(uint8_t*)(s + kStateKnife) = knife ? 1 : 0;
}

// ---------------------------------------------------------------------------
//  Кандидаты по одной точке: по одному на включённый хитбокс.
// ---------------------------------------------------------------------------

void BuildCandidates(void** out, void** outEnd, void* base, void* entry, int index, void* state)
{
    (void)entry;

    const int offListPointsBegin = 56;
    const int offListPointsEnd   = 64;

    const int hitboxMax = 5;

    if (!out || !outEnd)
        return;

    *out    = nullptr;
    *outEnd = nullptr;

    char* b = (char*)base;

    if (!b)
        return;

    char* pointsBegin = *(char**)(b + offListPointsBegin);
    char* pointsEnd   = *(char**)(b + offListPointsEnd);

    if (!pointsBegin || index < 0 || pointsBegin + kPointSize * index + kPointSize > pointsEnd)
        return;

    char* point = pointsBegin + kPointSize * index;

    float radius = *(const float*)(point + kPointRadius);

    if (!(radius > 0.f))
        radius = 1.f;

    const Vector position = *(const Vector*)(point + kPointPos);
    const Vector origin = state ? *(const Vector*)((char*)state + kStateOrigin) : Vector();

    uint32_t mask = state ? *(const uint32_t*)((char*)state + kStateMask) : 0u;

    if (!mask)
        mask = 0x7u;    // голова, грудь, живот

    int hitboxes[hitboxMax];
    int hitboxCount = 0;

    for (int i = 0; i < hitboxMax; ++i)
    {
        if (mask & (1u << i))
            hitboxes[hitboxCount++] = i;
    }

    if (!hitboxCount)
        return;

    const bool precise = g_settings && g_settings->doublePrecision;
    const int variants = precise ? 2 : 1;
    const int count = hitboxCount * variants;

    const size_t ptrBytes = sizeof(void*) * (size_t)count;
    char* block = (char*)malloc(ptrBytes + (size_t)kCandSize * (size_t)count);

    if (!block)
        return;

    void** array = (void**)block;
    char* data = block + ptrBytes;

    const float baseDamage = state ? *(const float*)((char*)state + kStateDamage) : 0.f;
    const float chance     = state ? *(const float*)((char*)state + kStateHitchance) : 0.f;
    const float distance   = state ? *(const float*)((char*)state + kStateDistance) : 0.f;
    const int   tick       = state ? *(const int*)((char*)state + kStateTick) : 0;

    int built = 0;

    for (int h = 0; h < hitboxCount; ++h)
    {
        const int hitbox = hitboxes[h];

        float offsetX = 0.f;
        float offsetZ = 0.f;
        part7_hitboxOffset(hitbox, radius, &offsetX, &offsetZ);

        for (int v = 0; v < variants; ++v)
        {
            char* candidate = data + (size_t)built * kCandSize;
            array[built] = candidate;

            Vector sample = position + Vector(offsetX, 0.f, offsetZ);

            // второй вариант смещаем вбок — покрываем разброс
            if (v == 1)
                sample = sample + Vector(radius * kLateralScale, radius * kLateralScale, 0.f);

            *(int*)(candidate + kCandHitbox)      = hitbox;
            *(float*)(candidate + kCandDamage)    = baseDamage * part7_hitboxScale(hitbox);
            *(float*)(candidate + kCandHitchance) = (v == 1) ? chance * kSecondVariant : chance;
            *(float*)(candidate + kCandDistance)  = distance;
            *(int*)(candidate + kCandFlags)       = 0;
            *(int*)(candidate + kCandIndex)       = index;
            *(Vector*)(candidate + kCandPoint)    = sample;
            *(Vector*)(candidate + kCandDir)      = (sample - origin).Normalized();
            *(uint8_t*)(candidate + kCandHit)     = 0;
            *(uint8_t*)(candidate + kCandWalls)   = 0;
            *(uint8_t*)(candidate + kCandGroup)   = (uint8_t)hitbox;
            *(int*)(candidate + kCandTick)        = tick;

            ++built;
        }
    }

    *out    = array;
    *outEnd = array + built;
}

// ---------------------------------------------------------------------------
//  Тест попадания по кандидату: видимость, хитшанс, лимит стен.
// ---------------------------------------------------------------------------

void TestAimHit(void* base, void* entry, void* point, void* candidate)
{
    char* cand = (char*)candidate;
    char* ent  = (char*)entry;

    if (!cand)
        return;

    *(uint8_t*)(cand + kCandHit)   = 0;
    *(uint8_t*)(cand + kCandWalls) = 0;

    if (!base || !point)
        return;

    // трасса до кандидата: false — точка перекрыта геометрией
    if (!RayTest(base, cand))
    {
        const int walls = *(const uint8_t*)(cand + kCandWalls) + 1;
        const int limit = g_settings ? g_settings->maxWalls : 0;

        *(uint8_t*)(cand + kCandWalls) = (uint8_t)(walls > 255 ? 255 : walls);

        if (walls > limit)
            return;

        // стена не отменяет кандидата, если лимит ещё не выбран,
        // но попаданием он не считается
        return;
    }

    // порог хитшанса из настроек: у записи он может быть уже перезаписан
    const float required = g_settings ? g_settings->hitchance : 0.f;
    const float chance   = *(const float*)(cand + kCandHitchance);

    if (required > 0.f && chance > 0.f && chance < required)
        return;

    // кандидат не должен выходить за радиус выбранной точки
    const Vector delta = *(const Vector*)(cand + kCandPoint)
                       - *(const Vector*)((char*)point + kPointPos);
    const float tolerance = *(const float*)((char*)point + kPointRadius);

    if (tolerance > 0.f && delta.Length() > tolerance)
        return;

    *(uint8_t*)(cand + kCandHit) = 1;

    if (ent && !*(int*)(cand + kCandTick))
        *(int*)(cand + kCandTick) = *(const int*)(ent + kEntryTick);
}

// ---------------------------------------------------------------------------
//  Сохранение попадания в запись цели и в список результатов.
// ---------------------------------------------------------------------------

void StoreAimHit(void* entry, void* candidate, void* point)
{
    char* e = (char*)entry;
    char* cand = (char*)candidate;
    char* p = (char*)point;

    if (!e || !cand || !p)
        return;

    if (!*(const uint8_t*)(cand + kCandHit))
        return;

    const float damage = *(const float*)(cand + kCandDamage);

    // уже есть попадание с большим уроном — оставляем его
    if (*(const uint8_t*)(e + kEntryHit) && *(const float*)(e + kEntryDamage) >= damage)
        return;

    // углы на точку
    const Vector origin = *(const Vector*)(e + kEntryOrigin);
    const Vector aim = *(const Vector*)(cand + kCandPoint) - origin;

    float angleX = 0.f;
    float angleY = 0.f;
    ComputeAngle(aim, &angleX, &angleY);

    float timePair[2] = { 0.f, 0.f };
    const float timeFrac = GetTimeFrac(1.f, timePair);
    const float frac = (timePair[0] != 0.f) ? timePair[0] : timeFrac;

    const int tick = *(const int*)(cand + kCandTick);
    const int currentTick = *(const int*)(e + kEntryTick);

    float delay = 0.f;

    if (currentTick > tick)
        delay = 1.f;
    else if (frac > 0.f)
        delay = 1.f - frac;

    if (delay < 0.f)
        delay = 0.f;

    if (delay > 1.f)
        delay = 1.f;

    // запись цели
    *(uint8_t*)(e + kEntryHit)   = 1;
    *(uint8_t*)(e + kEntryWalls) = *(const uint8_t*)(cand + kCandWalls);
    *(float*)(e + kEntryDamage)  = damage;
    *(float*)(e + kEntryHitchance) = *(const float*)(cand + kCandHitchance);
    *(float*)(e + kEntryTimeFrac)  = frac;
    *(float*)(e + kEntryDelayFrac) = delay;
    *(int*)(e + kEntryTick)        = tick;
    *(int*)(e + kEntryIndex)       = *(const int*)(cand + kCandIndex);
    *(float*)(e + kEntryAngleMax)  = *(const float*)(p + kPointAngleMax);
    *(void**)(e + kEntryPointData) = p;

    *(float*)(e + kEntryAngles) = angleX;
    *(float*)(e + kEntryAngles + 4) = angleY;
    *(int*)(e + kEntryAngleTag) =
        (int)HashFloat((uint32_t)(angleX * kAngleHashScale) ^ (uint32_t)(angleY * kAngleHashScale));

    // буфер попадания
    char* hit = *(char**)(e + kEntryHitData);

    if (hit)
    {
        const uint32_t tag = (uint32_t)*(const int*)(e + kEntryAngleTag);

        *(int*)(hit + kHitTick)     = tick;
        *(float*)(hit + kHitFrac)   = frac;
        *(uint32_t*)(hit + kHitTag) = tag;
        *(uint8_t*)(hit + kHitHasAngles) = 1;
        *(uint8_t*)(hit + kHitGroup)     = *(const uint8_t*)(cand + kCandGroup);

        *(float*)(hit + kHitAngles)     = angleX;
        *(float*)(hit + kHitAngles + 4) = angleY;

        *(Vector*)(hit + kHitPoint)  = *(const Vector*)(cand + kCandPoint);
        *(float*)(hit + kHitDistance) = *(const float*)(cand + kCandDistance);
        *(int*)(hit + kHitDamage)     = (int)damage;
        *(int*)(hit + kHitbox)        = *(const int*)(cand + kCandHitbox);
        *(int*)(hit + kHitWalls)      = *(const uint8_t*)(cand + kCandWalls);
        *(uint8_t*)(hit + kHitValid)  = 1;
        *(float*)(hit + kHitChance)   = *(const float*)(cand + kCandHitchance);
        *(int*)(hit + kHitIndex)      = *(const int*)(cand + kCandIndex);
        *(int*)(hit + kHitAge)        = (currentTick > tick) ? (currentTick - tick) : 0;

        *(int*)(hit + kHitDamageAlt)  = (int)damage;
        *(int*)(hit + kHitAnglesAlt)  = (int)*(const int*)(e + kEntryAngleTag);
        *(uint8_t*)(hit + kHitResolved) = 1;
    }

    // публикуем результат отбора
    part7_registerResult(e);
}

// ---------------------------------------------------------------------------
//  Освобождение массива кандидатов (кандидаты лежат в том же блоке).
// ---------------------------------------------------------------------------

void ReleaseCandidates(void* candidates)
{
    if (candidates)
        free(candidates);
}

// ---------------------------------------------------------------------------
//  Подготовка записи команды для списка прицеливания.
// ---------------------------------------------------------------------------

void PrepareAimRecord(void* ctx, void* record)
{
    const int stride   = 30;
    const int count    = 17;
    const int offTick  = 2820;
    const int offId    = 2800;
    const int offAngles = 164;
    const int offMode  = 96;
    const int offFlags = 4488;
    const int offAmmo  = 2641;

    const int offElemTick    = 0;
    const int offElemFlags   = 4;
    const int offElemCommand = 8;
    const int offElemAngles  = 12;
    const int offElemTime    = 24;
    const int offElemState   = 28;

    char* c = (char*)ctx;
    char* r = (char*)record;

    if (!r)
        return;

    memset(r, 0, (size_t)stride * (size_t)count);

    const int tick = c ? *(const int*)(c + offTick) : 0;
    const uint64_t id = c ? *(const uint64_t*)(c + offId) : 0;
    const int flags = c ? (int)*(const uint32_t*)(c + offFlags) : 0;
    const int mode = c ? *(const int*)(c + offMode) : 0;
    const int ammo = c ? *(const int*)(c + offAmmo) : 0;

    for (int i = 0; i < count; ++i)
    {
        char* elem = r + (size_t)stride * (size_t)i;

        *(int*)(elem + offElemTick)    = tick - i;
        *(int*)(elem + offElemFlags)   = flags;

        int command = (int)((id >> 32) ^ (uint64_t)(tick - i));

        if (!command)
            command = 1;

        *(int*)(elem + offElemCommand) = command;
        *(float*)(elem + offElemTime)  = (float)(tick - i) * kTickToTime;

        if (c)
        {
            *(Vector*)(elem + offElemAngles) = *(const Vector*)(c + offAngles);
        }

        // первая запись — текущая команда
        if (i == 0)
        {
            *(uint8_t*)(elem + offElemState) = (mode == 2 || ammo > 0) ? 1 : 2;
        }
        else
        {
            *(uint8_t*)(elem + offElemState) = 0;
        }
    }
}

// ---------------------------------------------------------------------------
//  Проверка пригодности записи команды.
// ---------------------------------------------------------------------------

bool CheckAimRecord(void* record, int mode, uint8_t silent)
{
    (void)mode;

    const int stride = 30;
    const int count  = 17;

    const int offElemTick    = 0;
    const int offElemCommand = 8;
    const int offElemAngles  = 12;
    const int offElemState   = 28;

    char* r = (char*)record;

    if (!r)
        return false;

    const int tick = *(const int*)(r + offElemTick);

    if (tick <= 0)
        return false;

    if (!*(const int*)(r + offElemCommand))
        return false;

    const uint8_t state = *(const uint8_t*)(r + offElemState);

    // состояние 1 — обычная запись, 2 — допустима только в silent
    if (state != 1 && !(state == 2 && silent))
        return false;

    // углы должны быть конечными
    const float angleX = *(const float*)(r + offElemAngles);
    const float angleY = *(const float*)(r + offElemAngles + 4);

    if (!(angleX == angleX) || !(angleY == angleY))
        return false;

    if (fabsf(angleX) > 360.f || fabsf(angleY) > 360.f)
        return false;

    // тики истории должны убывать
    for (int i = 1; i < count; ++i)
    {
        const int previous = *(const int*)(r + (size_t)stride * (size_t)(i - 1) + offElemTick);
        const int current  = *(const int*)(r + (size_t)stride * (size_t)i + offElemTick);

        if (current && current >= previous)
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  Пересборка записи при смене режима.
// ---------------------------------------------------------------------------

void* RebuildAimEntry(void* list)
{
    const int offBegin = 224;
    const int offEnd   = 232;

    char* b = (char*)list;

    if (!b)
        return nullptr;

    part7_ensureResults(b);

    char* begin = *(char**)(b + offBegin);
    char* end   = *(char**)(b + offEnd);

    char* best = nullptr;

    if (begin && end && end > begin)
    {
        for (char* it = begin; it + kResultSize <= end; it += kResultSize)
        {
            if (!*(const uint8_t*)(it + kResultHit))
                continue;

            if (!best)
            {
                best = it;
                continue;
            }

            const float damage = *(const float*)(it + kResultDamage);
            const float bestDamage = *(const float*)(best + kResultDamage);

            if (damage > bestDamage)
            {
                best = it;
                continue;
            }

            if (damage == bestDamage
                && *(const float*)(it + kResultHitchance) > *(const float*)(best + kResultHitchance))
            {
                best = it;
            }
        }
    }

    if (!best)
        return nullptr;

    SelectAimEntry(b, best);
    FinishAimList(b);

    return best;
}

// ---------------------------------------------------------------------------
//  Сортировка и подготовка точек списка.
// ---------------------------------------------------------------------------

void SortAimPoints(void* ctx, void* list, void* record)
{
    const int offCtxOrigin = 0;
    const int offCtxMode   = 96;
    const int offCtxKnife  = 208;

    const int offListPointsBegin = 56;
    const int offListPointsEnd   = 64;
    const int offListValid       = 80;
    const int offListRecId       = 108;
    const int offListRecTick     = 116;
    const int offListFlag        = 132;
    const int offListMode        = 172;
    const int offListCount       = 204;

    const int pointMax  = 32;

    char* c = (char*)ctx;
    char* b = (char*)list;

    if (!b)
        return;

    part7_ensureResults(b);

    char* begin = *(char**)(b + offListPointsBegin);
    char* end   = *(char**)(b + offListPointsEnd);

    if (!begin || !end || end < begin)
    {
        *(int*)(b + offListCount) = 0;
        *(uint8_t*)(b + offListValid) = 0;
        return;
    }

    int count = (int)((end - begin) / kPointSize);

    if (count > pointMax)
        count = pointMax;

    // выбрасываем точки с битым указателем на цель
    int kept = 0;

    for (int i = 0; i < count; ++i)
    {
        char* point = begin + (size_t)kPointSize * (size_t)i;
        void* target = *(void**)(point + kPointTarget);

        if (target && (!IsValid(target) || !IsValid((const char*)target + 16)))
            continue;

        if (kept != i)
            memcpy(begin + (size_t)kPointSize * (size_t)kept, point, kPointSize);

        ++kept;
    }

    count = kept;

    const Vector origin = c ? *(const Vector*)(c + offCtxOrigin) : Vector();

    // сортировка вставками: ближние точки обрабатываются первыми
    for (int i = 1; i < count; ++i)
    {
        char current[kPointSize];
        memcpy(current, begin + (size_t)kPointSize * (size_t)i, kPointSize);

        int j = i - 1;

        while (j >= 0 && part7_pointLess(current, begin + (size_t)kPointSize * (size_t)j, origin))
        {
            memcpy(begin + (size_t)kPointSize * (size_t)(j + 1),
                   begin + (size_t)kPointSize * (size_t)j, kPointSize);
            --j;
        }

        memcpy(begin + (size_t)kPointSize * (size_t)(j + 1), current, kPointSize);
    }

    *(char**)(b + offListPointsEnd) = begin + (size_t)kPointSize * (size_t)count;

    *(int*)(b + offListCount) = count;
    *(uint8_t*)(b + offListValid) = count > 0 ? 1 : 0;

    if (c)
    {
        *(int*)(b + offListMode) = *(const int*)(c + offCtxMode);

        const bool knife = *(const int*)(c + offCtxMode) == 2
                        && *(const uint8_t*)(c + offCtxKnife) != 0;

        *(uint8_t*)(b + offListFlag) = (count > 0 && !knife) ? 1 : 0;
    }

    if (record)
    {
        *(uint64_t*)(b + offListRecId)   = *(const uint64_t*)record;
        *(int*)(b + offListRecTick)      = *(const int*)((char*)record + 8);
    }
}

// ---------------------------------------------------------------------------
//  Телеметрия выстрела: снимок углов и текущего тика.
// ---------------------------------------------------------------------------

void UpdateShotTelemetry(void* angles)
{
    const int offTelemetryX = 16;
    const int offTelemetryY = 20;
    const int offTelemetryZ = 24;
    const int offTelemetryReady = 28;

    const float* view = (const float*)angles;

    if (!view)
        return;

    // телеметрия имеет смысл только при активном трекинге
    if (g_shotState[0] == 0.f)
        return;

    *(float*)&g_shotInfo[offTelemetryX] = view[0];
    *(float*)&g_shotInfo[offTelemetryY] = view[1];
    *(float*)&g_shotInfo[offTelemetryZ] = view[2];

    g_shotInfo[offTelemetryReady] = 1;

    MARKER(1);
}

// ---------------------------------------------------------------------------
//  Завершение прохода: фиксируем итог в состоянии и телеметрии.
// ---------------------------------------------------------------------------

void FinishShotSolve(void* ctx)
{
    const int offAimAngles = 164;
    const int offTick      = 2820;

    char* c = (char*)ctx;

    if (!c)
        return;

    CopyViewAngles(g_aimAngles, c + offAimAngles);
    UpdateShotTelemetry(g_aimAngles);

    if (g_result)
        g_result->resolved = 1;

    if (g_rage)
    {
        g_rage->lastTick = *(const int*)(c + offTick);
        g_rage->lastTime = (float)*(const int*)(c + offTick) * GetTickInterval(1.f);
    }

    g_shotDirty = true;
    g_shotMarker0.active = 1;
    g_shotMarker1.active = 1;

    if (g_shotState[0] != 0.f)
    {
        g_shotInfo[1] = 1;
        *(int*)&g_shotInfo[4] = *(const int*)(c + offTick);
    }

    MARKER(7);
}


// ============================================================================
//  добавлено: part8.cpp
// ============================================================================
// GetModuleHandleA/GetProcAddress — резолв экспорта g_pThreadPool из tier0.
#include <windows.h>

// ============================================================================
//  part8 — запросы на просчёт, батч прицеливания и учёт выстрела
//
//  Здесь живут: сборка запроса (углы -> плоский запрос -> очередь), перенос
//  результата в контекст, снимок батча прицеливания, разбор разброса и
//  предсказание точки на будущий тик.
// ============================================================================

// Шаги элементов в плоских списках rage.
constexpr int part8_aimEntryStep   = 72;   // запись прицеливания в запросе
constexpr int part8_shotPointStep  = 12;   // точка выстрела: два угла + флаг
constexpr int part8_aimSlotStep    = 172;  // запись слота прицеливания
constexpr int part8_shotOriginStep = 96;   // запись начала выстрела

// Плоский список: начало, конец, ёмкость. Владеет буфером.
struct part8_RawVector
{
    char* begin;
    char* end;
    char* capacity;
};

// Запрос на просчёт точки. Живёт на стеке вызывающего, ровно 128 байт.
struct part8_SolveRequest
{
    int32_t         mode;        // 0x00: режим прохода
    int32_t         slot;        // 0x04: слот, из которого взяты точки
    void*           origin;      // 0x08
    void*           view;        // 0x10
    void*           buttons;     // 0x18
    int32_t         data[6];     // 0x20: снимок углов и тиков
    int32_t         itemIndex;   // 0x38: индекс предмета оружия
    char            pad0[4];     // 0x3C
    part8_RawVector entries;     // 0x40: записи прицеливания, шаг 72
    part8_RawVector points;      // 0x58: точки выстрела, шаг 12
    void*           payload;     // 0x70
    int32_t         field78;     // 0x78
    uint8_t         flag7C;      // 0x7C
    uint8_t         flag7D;      // 0x7D
    uint8_t         flag7E;      // 0x7E
};

static_assert(sizeof(part8_SolveRequest) == 128, "solve request layout");
static_assert(offsetof(part8_SolveRequest, entries) == 0x40, "request entries");
static_assert(offsetof(part8_SolveRequest, points) == 0x58, "request points");

// Снимок, из которого собирается запрос на предсказание.
struct part8_PredictSource
{
    int32_t         data[6];     // 0x00
    void*           payload;     // 0x18
    int32_t         field20;     // 0x20
    uint8_t         flag24;      // 0x24
    uint8_t         flag25;      // 0x25
    char            pad0[2];     // 0x26
    void*           origin;      // 0x28
    void*           view;        // 0x30
    part8_RawVector entries;     // 0x38: записи прицеливания, шаг 72
};

static_assert(sizeof(part8_PredictSource) == 80, "predict source layout");

// Хвост батча прицеливания: то, что читают воркеры на каждом проходе.
// Смещения от батча — 1860, поэтому первое поле соответствует 0x744.
// База 1860 не выровнена на 8, поэтому структура упакована по 4 байта.
#pragma pack(push, 4)
struct part8_AimBatchTail
{
    uint8_t  debounceZoom;   // 1860 (0x00): cl_debounce_zoom
    char     pad0[3];
    float    timeFrac;       // 1864 (0x04): доля времени до выстрела, [0,1)
    uint8_t  throwable;      // 1868 (0x08): по объекту ещё можно стрелять
    char     pad1[3];
    char     pad2[8];        // 1872 (0x0C)
    Vector   angles;         // 1880 (0x14): углы текущего прохода
    int32_t  field1C;        // 1892 (0x20)
    uint8_t  flag20;         // 1896 (0x24)
    uint8_t  flag21;         // 1897 (0x25)
    uint8_t  flag22;         // 1898 (0x26)
    char     pad3[1];        // 1899 (0x27)
    int32_t  field24;        // 1900 (0x28)
    int32_t  field28;        // 1904 (0x2C)
    int32_t  field2C;        // 1908 (0x30)
    float    jumpImpulse;    // 1912 (0x34): sv_jump_impulse
    int32_t  field34;        // 1916 (0x38)
    int32_t  field38;        // 1920 (0x3C)
    char     pad4[0x14];     // 1924 (0x40)
    float    airSpread;      // 1944 (0x54): weapon_air_spread_scale
    float    forceSpread;    // 1948 (0x58): weapon_accuracy_forcespread
    uint8_t  noSpread;       // 1952 (0x5C): weapon_accuracy_nospread
    char     pad5[7];        // 1953 (0x5D)
    void*    weapon;         // 1960 (0x64)
    void*    weaponData;     // 1968 (0x6C)
    uint16_t weaponId;       // 1976 (0x74)
};
#pragma pack(pop)

static_assert(offsetof(part8_AimBatchTail, angles)     == 0x14, "batch angles");
static_assert(offsetof(part8_AimBatchTail, jumpImpulse) == 0x34, "batch impulse");
static_assert(offsetof(part8_AimBatchTail, airSpread)   == 0x54, "batch spread");
static_assert(offsetof(part8_AimBatchTail, weapon)      == 0x64, "batch weapon");
static_assert(offsetof(part8_AimBatchTail, weaponData)  == 0x6C, "batch weapon data");
static_assert(offsetof(part8_AimBatchTail, weaponId)    == 0x74, "batch weapon id");

// Состояние учёта выстрела: контекст получает его из шаблона «нет данных».
struct part8_ShotBookkeeping
{
    uint8_t         empty0[0x20];   // 0x00: два блока по 16 байт
    int32_t         field20;        // 0x20
    uint16_t        field24;        // 0x24
    char            pad0[2];        // 0x26
    int64_t         field28;        // 0x28
    int64_t         field30;        // 0x30
    part8_RawVector entries;        // 0x38: записи прицеливания, шаг 72
    int32_t         field50;        // 0x50
    char            pad1[4];        // 0x54
    part8_RawVector points;         // 0x58: точки выстрела, шаг 172
    int32_t         field70;        // 0x70
    int32_t         field74;        // 0x74
    int32_t         field78;        // 0x78
    int32_t         field7C;        // 0x7C
    uint8_t         flag80;         // 0x80
};

static_assert(offsetof(part8_ShotBookkeeping, entries) == 0x38, "bookkeeping entries");
static_assert(offsetof(part8_ShotBookkeeping, points)  == 0x58, "bookkeeping points");

// ---------------------------------------------------------------------------
//  плоские списки
// ---------------------------------------------------------------------------

// Освобождает буфер списка.

static void part8_ClearVector(part8_RawVector* list)
{
    if (list->begin)
        free(list->begin);

    list->begin    = nullptr;
    list->end      = nullptr;
    list->capacity = nullptr;
}

// Присваивание как у std::vector: старый буфер освобождается, элементы
// копируются в новый ровно по размеру.

static void part8_CopyVector(part8_RawVector* dst, const part8_RawVector* src, size_t step)
{
    const size_t count = (size_t)(src->end - src->begin) / step;

    part8_ClearVector(dst);

    if (!count)
        return;

    char* fresh = (char*)malloc(count * step);
    if (!fresh)
        return;

    memcpy(fresh, src->begin, count * step);

    dst->begin    = fresh;
    dst->end      = fresh + count * step;
    dst->capacity = fresh + count * step;
}

// Перемещающее присваивание: буфер переходит к приёмнику, источник пустеет.

static void part8_MoveVector(part8_RawVector* dst, part8_RawVector* src)
{
    part8_ClearVector(dst);

    *dst = *src;

    src->begin    = nullptr;
    src->end      = nullptr;
    src->capacity = nullptr;
}

// reserve: ёмкость растёт до запрошенной, элементы сохраняются.

static void part8_ReserveVector(part8_RawVector* list, unsigned count, size_t step)
{
    const size_t capacity = (size_t)(list->capacity - list->begin) / step;
    if ((size_t)count <= capacity)
        return;

    const size_t used  = (size_t)(list->end - list->begin) / step;
    char*        fresh = (char*)malloc((size_t)count * step);
    if (!fresh)
        return;

    if (used)
        memcpy(fresh, list->begin, used * step);

    if (list->begin)
        free(list->begin);

    list->begin    = fresh;
    list->end      = fresh + used * step;
    list->capacity = fresh + (size_t)count * step;
}

// push_back: при переполнении ёмкость растёт в полтора раза.

static void part8_AppendVector(part8_RawVector* list, const void* value, size_t step)
{
    if (list->end == list->capacity)
    {
        const size_t used     = (size_t)(list->end - list->begin) / step;
        const size_t capacity = (size_t)(list->capacity - list->begin) / step;
        const size_t maxCount = (size_t)-1 / step;

        size_t grown = capacity + (capacity >> 1);
        if (grown < used + 1)
            grown = used + 1;
        if (grown > maxCount)
            grown = maxCount;

        char* fresh = (char*)malloc(grown * step);
        if (!fresh)
            return;

        if (used)
            memcpy(fresh, list->begin, used * step);

        if (list->begin)
            free(list->begin);

        list->begin    = fresh;
        list->end      = fresh + used * step;
        list->capacity = fresh + grown * step;
    }

    memcpy(list->end, value, step);
    list->end += step;
}

// ---------------------------------------------------------------------------
//  предсказание точки прицеливания
// ---------------------------------------------------------------------------

// Позиция на будущий тик: движение с затуханием, шаг 1/64 тика.
// В бинаре на каждом шаге подмешивается запись из истории тиков; история сюда
// не передаётся, поэтому опорой служит сама точка, а скорость берётся от
// начала координат.

static Vector part8_PredictPosition(int baseTick, float time, const Vector& point,
                                    const Vector& origin, int tick, float tickTime)
{
    const float stepTime = 0.015625f;   // 1/64 тика
    const float decay    = 0.93941301f; // exp(-1/16): затухание скорости за шаг
    const float deadZone = 0.140625f;   // ближе этого точка не двигается
    const int   maxSteps = 128;

    float frac = time;
    int   base = baseTick;

    // время вне [0,1) разворачивается в соседний тик
    if (frac < 0.f || frac >= 1.f)
    {
        float whole = 0.f;
        frac = GetTimeFrac(frac, &whole);
        base += (int)whole;

        if (frac < 0.f)
            frac += 1.f;
    }

    const float span = (float)(tick - base) + (tickTime - frac);
    if (span <= 0.f)
        return point;

    int steps = (int)(span * stepTime * (float)maxSteps) + 1;
    if (steps > maxSteps)
        steps = maxSteps;

    Vector velocity  = (point - origin) * stepTime;
    Vector predicted = point;

    for (int i = 0; i < steps; ++i)
    {
        velocity = velocity * decay;

        if (velocity.Length() > deadZone)
            predicted = predicted + velocity;
    }

    return predicted;
}

// Углы с начала координат на предсказанную точку: [0] — yaw, [1] — pitch.

static Vector part8_PredictAngles(int baseTick, float time, const float* point,
                                  const float* origin, int tick, float tickTime,
                                  bool doubled)
{
    const float doubleScale = 2.f;

    if (!point || !origin)
        return Vector();

    const Vector from(origin[0], origin[1], origin[2]);
    const Vector to(point[0], point[1], point[2]);

    const Vector predicted = part8_PredictPosition(baseTick, time, to, from, tick, tickTime);

    float pitch = 0.f;
    float yaw   = 0.f;

    ComputeAngle(predicted - from, &pitch, &yaw);

    Vector angles(yaw, pitch, 0.f);
    if (doubled)
        angles = angles * doubleScale;

    return angles;
}

// ---------------------------------------------------------------------------
//  сборка запросов
// ---------------------------------------------------------------------------

// Заполняет запрос предсказания: обнуляет его и переносит снимок.

void BuildPredictRequest(void* out, void* angles, void* buttons, int mode, int flag)
{
    part8_SolveRequest*        request = (part8_SolveRequest*)out;
    const part8_PredictSource* src     = (const part8_PredictSource*)angles;

    memset(request, 0, sizeof(*request));

    request->mode     = mode;
    request->origin   = src->origin;
    request->view     = src->view;
    request->buttons  = buttons;
    request->payload  = src->payload;
    request->field78  = src->field20;
    request->flag7C   = src->flag24;
    request->flag7D   = src->flag25;
    request->flag7E   = (uint8_t)flag;

    memcpy(request->data, src->data, sizeof(request->data));

    part8_CopyVector(&request->entries, &src->entries, part8_aimEntryStep);
}

// Заполняет запрос просчёта: предсказание плюс точки выбранного слота.

void* BuildSolveRequest(void* out, void* rage, void* angles, void* buttons, void* slot)
{
    const int offMode       = 188;   // режим прохода в контексте
    const int offWeapon     = 4456;  // оружие, которым считаем
    const int offWeaponData = 64;
    const int offItemIndex  = 108;
    const int offSlotBegin  = 8;
    const int offSlotEnd    = 16;
    const int offAngleX     = 24;    // координаты угла в записи слота
    const int offAngleY     = 28;
    const int offValid      = 32;

    part8_SolveRequest* request = (part8_SolveRequest*)out;

    BuildPredictRequest(out, angles, buttons, *(const int32_t*)((char*)rage + offMode), 0);

    request->slot = *(const int32_t*)slot;

    const char*    first = *(char* const*)((char*)slot + offSlotBegin);
    const char*    last  = *(char* const*)((char*)slot + offSlotEnd);
    const unsigned count = (unsigned)((last - first) / part8_aimSlotStep);

    BuildPosList(&request->points, count);

    for (const char* it = first; it != last; it += part8_aimSlotStep)
    {
        struct part8_PosEntry
        {
            int32_t x;
            int32_t y;
            uint8_t valid;
        } entry;

        entry.x     = *(const int32_t*)(it + offAngleX);
        entry.y     = *(const int32_t*)(it + offAngleY);
        entry.valid = *(const uint8_t*)(it + offValid);

        AppendPos(&request->points, &entry);
    }

    // индекс предмета оружия: по нему выбирается модель просчёта
    void* weapon = *(void**)((char*)rage + offWeapon);
    if (weapon)
    {
        char* weaponData = (char*)ReadPtr((char*)weapon + offWeaponData);
        if (IsValidAddr((uintptr_t)weaponData + offItemIndex))
            request->itemIndex = ReadInt(weaponData + offItemIndex);
    }

    return request;
}

// Кладёт результат в очередь: записи копируются, точки переезжают.

void QueueSolveResult(void* queue, void* result)
{
    part8_SolveRequest* dst = (part8_SolveRequest*)queue;
    part8_SolveRequest* src = (part8_SolveRequest*)result;

    dst->mode      = src->mode;
    dst->slot      = src->slot;
    dst->origin    = src->origin;
    dst->view      = src->view;
    dst->buttons   = src->buttons;
    dst->itemIndex = src->itemIndex;
    dst->payload   = src->payload;
    dst->field78   = src->field78;
    dst->flag7C    = src->flag7C;
    dst->flag7D    = src->flag7D;
    dst->flag7E    = src->flag7E;

    memcpy(dst->data, src->data, sizeof(dst->data));

    if (dst != src)
    {
        part8_CopyVector(&dst->entries, &src->entries, part8_aimEntryStep);
        part8_MoveVector(&dst->points, &src->points);
    }
}

// Переносит запрос в контекст: оба списка копируются.

void MergeSolveResult(void* dstOut, void* requestOut)
{
    part8_SolveRequest* dst = (part8_SolveRequest*)dstOut;
    part8_SolveRequest* src = (part8_SolveRequest*)requestOut;

    dst->mode      = src->mode;
    dst->slot      = src->slot;
    dst->origin    = src->origin;
    dst->view      = src->view;
    dst->buttons   = src->buttons;
    dst->itemIndex = src->itemIndex;

    memcpy(dst->data, src->data, sizeof(dst->data));

    if (dst != src)
    {
        part8_CopyVector(&dst->entries, &src->entries, part8_aimEntryStep);
        part8_CopyVector(&dst->points, &src->points, part8_shotPointStep);
    }

    dst->payload = src->payload;
    dst->field78 = src->field78;
    dst->flag7C  = src->flag7C;
    dst->flag7D  = src->flag7D;
    dst->flag7E  = src->flag7E;
}

// ---------------------------------------------------------------------------
//  пул потоков tier0
// ---------------------------------------------------------------------------

// Адрес экспорта g_pThreadPool резолвится один раз, дальше берётся объект.

void* GetThreadPool()
{
    static void* exportSlot = nullptr;
    static bool  resolved   = false;

    if (!resolved)
    {
        resolved = true;

        HMODULE tier0 = GetModuleHandleA("tier0.dll");
        if (tier0)
            exportSlot = (void*)GetProcAddress(tier0, "g_pThreadPool");
    }

    if (!exportSlot)
        return nullptr;

    return *(void**)exportSlot;
}

// Число воркеров пула: виртуальный метод по смещению 56.

int GetPoolWorkers(void* pool)
{
    const int workerSlot = 56 / (int)sizeof(void*);

    if (!pool)
        return 1;

    void** vtable = *(void***)pool;
    if (!vtable)
        return 1;

    typedef int (*WorkersFn)(void*);

    WorkersFn workers = (WorkersFn)vtable[workerSlot];
    return workers ? workers(pool) : 1;
}

// ---------------------------------------------------------------------------
//  батч прицеливания
// ---------------------------------------------------------------------------

// Снимает состояние прохода в хвост батча и отдаёт его пулу. Мелкий батч
// (или пул в один поток) считается прямо здесь, на игровом потоке.

void DispatchAimBatch(void* pool, void* batch, int limit, int offset, int mode)
{
    const int offTail      = 1860;
    const int offAngleX    = 12;
    const int offAngleY    = 16;
    const int offAngleZ    = 20;
    const int offField104  = 104;
    const int offFlag112   = 112;
    const int offFlag114   = 114;
    const int offFlag170   = 170;
    const int offOwner     = 336;   // владелец, по нему считается время выстрела
    const int offField348  = 348;
    const int offField356  = 356;
    const int offField360  = 360;
    const int offField724  = 724;
    const int offHandle    = 844;   // хэндл оружия
    const int offField860  = 860;
    const int offField864  = 864;
    const int offSubclass  = 8;     // VData лежит за смещением подобъекта

    // Умолчания конваров, которые снимаются на каждом проходе:
    // weapon_air_spread_scale, weapon_accuracy_forcespread,
    // weapon_accuracy_nospread, sv_jump_impulse, cl_debounce_zoom.
    const float airSpreadScale = 1.f;
    const float forceSpread    = 0.f;
    const bool  noSpread       = false;
    const float jumpImpulse    = 301.87f;
    const bool  debounceZoom   = false;

    char*               src  = (char*)batch;
    part8_AimBatchTail* tail = (part8_AimBatchTail*)(src + offTail);

    // смещения оружия нужны ниже — резолвим их, если проход ещё не делал этого
    ResetAimState();

    // снимок углов и признаков прохода
    tail->angles   = Vector(*(const float*)(src + offAngleX),
                            *(const float*)(src + offAngleY),
                            *(const float*)(src + offAngleZ));
    tail->field1C  = *(const int32_t*)(src + offField104);
    tail->flag20   = *(const uint8_t*)(src + offFlag112);
    tail->flag21   = *(const uint8_t*)(src + offFlag114);
    tail->flag22   = *(const uint8_t*)(src + offFlag170);
    tail->field24  = *(const int32_t*)(src + offField348);
    tail->field28  = *(const int32_t*)(src + offField356);
    tail->field2C  = *(const int32_t*)(src + offField360);
    tail->field34  = *(const int32_t*)(src + offField860);
    tail->field38  = *(const int32_t*)(src + offField864);

    tail->airSpread    = airSpreadScale;
    tail->forceSpread  = forceSpread;
    tail->noSpread     = noSpread ? 1 : 0;
    tail->debounceZoom = debounceZoom ? 1 : 0;

    // снятый из батча импульс перекрывается конваром sv_jump_impulse,
    // но только если конвар задан
    const float batchImpulse = *(const float*)(src + offField724);

    tail->jumpImpulse = (jumpImpulse > 0.f) ? jumpImpulse : batchImpulse;

    // оружие по хэндлу: сама сущность, её VData и индекс предмета
    tail->weapon     = nullptr;
    tail->weaponData = nullptr;
    tail->weaponId   = 0;
    tail->throwable  = 0;

    const int32_t handle = *(const int32_t*)(src + offHandle);
    if ((uint32_t)(handle - 1) <= 0xFFFFFFFDu)
    {
        void* entity = GetEntityFromHandle(handle);
        if (entity)
        {
            tail->weapon = entity;

            if (g_aimOffsetA)
            {
                void* data = ReadPtr((char*)entity + g_aimOffsetA + offSubclass);
                if (IsValid(data))
                    tail->weaponData = data;
            }

            if (g_aimOffsetG && g_aimOffsetH && g_aimOffsetI)
                tail->weaponId = ReadWord((char*)entity + g_aimOffsetG + g_aimOffsetH + g_aimOffsetI);

            tail->throwable = CanStillShoot(entity) ? 1 : 0;
        }
    }

    // доля времени до выстрела: годится только [0,1)
    const float frac = GetShotTime(*(void**)(src + offOwner));

    if (std::isfinite(frac) && frac >= 0.f && frac < 1.f)
        tail->timeFrac = frac;
    else
        tail->timeFrac = 0.f;

    // Воркеров меньше, чем записей, — пул не нужен. Смещение начала не
    // используется: RunAimBatch всегда идёт с начала списка.
    const int workers = pool ? GetPoolWorkers(pool) : 1;

    (void)offset;

    if (workers <= 1 || (limit > 0 && limit <= workers))
        RunAimBatch(batch, mode);
}

// ---------------------------------------------------------------------------
//  учёт выстрела
// ---------------------------------------------------------------------------

// Сбрасывает учёт выстрела в контексте на шаблон «нет данных».

void UpdateShotBookkeeping(void* rage)
{
    const int offBookkeeping = 2128;

    const int32_t noData = -1;
    const uint16_t noDataWord = 0xFFFF;

    part8_ShotBookkeeping* book =
        (part8_ShotBookkeeping*)((char*)rage + offBookkeeping);

    memset(book->empty0, 0xFF, sizeof(book->empty0));

    book->field20 = noData;
    book->field24 = noDataWord;
    book->field28 = noData;
    book->field30 = noData;

    // оба списка шаблон держит пустыми
    part8_ClearVector(&book->entries);
    book->field50 = 0;

    part8_ClearVector(&book->points);

    book->field70 = noData;
    book->field74 = 0;
    book->field78 = noData;
    book->field7C = 0;
    book->flag80  = 0;
}

// База модели урона — время цикла оружия: у ножа оно лежит во втором слоте.

float ComputeDamage(void* ctx, bool knife)
{
    static bool resolved     = false;
    static int  offCycleTime = 0;
    static int  offWeaponType = 0;

    const int offWeaponData = 1968;   // оружие в контексте просчёта
    const int slotStep      = (int)sizeof(float);

    if (!resolved && g_schemaSystem)
    {
        resolved      = true;
        offCycleTime  = SchemaOffset("CCSWeaponBaseVData", "m_flCycleTime");
        offWeaponType = SchemaOffset("CCSWeaponBaseVData", "m_WeaponType");
    }

    if (!offCycleTime)
        return 0.f;

    const char* weaponData = *(char* const*)((char*)ctx + offWeaponData);
    if (!IsValid(weaponData))
        return 0.f;

    const int index = knife ? 1 : 0;

    return ReadFloat(weaponData + index * slotStep + offCycleTime);
}

// ---------------------------------------------------------------------------
//  списки точек
// ---------------------------------------------------------------------------

// Добавляет начало выстрела в список (запись 96 байт).

void PushShotOrigin(void* list, void* origin)
{
    part8_AppendVector((part8_RawVector*)list, origin, part8_shotOriginStep);
}

// Готовит список точек выстрела под count записей по 12 байт.

void BuildPosList(void* out, unsigned count)
{
    part8_ReserveVector((part8_RawVector*)out, count, part8_shotPointStep);
}

// ---------------------------------------------------------------------------
//  углы
// ---------------------------------------------------------------------------

// Углы направления: pitch в x, yaw в y. Оба приводятся к [-180,180).

void ComputeAngle(const Vector& d, float* x, float* y)
{
    const float radToDeg  = 180.f / 3.1415927f;
    const float halfTurn  = 360.f;
    const float roundBias = 0.5f;

    if (d.x == 0.f && d.y == 0.f)
    {
        // строго вертикальное направление
        *x = (d.z > 0.f) ? 270.f : 90.f;
        *y = 0.f;
    }
    else
    {
        *y = atan2f(d.y, d.x) * radToDeg;
        *x = atan2f(-d.z, sqrtf(d.x * d.x + d.y * d.y)) * radToDeg;

        if (*y < 0.f)
            *y += halfTurn;
        if (*x < 0.f)
            *x += halfTurn;
    }

    *x -= GetAngleScale(*x / halfTurn + roundBias) * halfTurn;
    *y -= GetAngleScale(*y / halfTurn + roundBias) * halfTurn;
}

// Поправка на предсказание двух точек контекста: складывается и возвращается
// в буфер d, откуда вызывающий её вычитает.

void* AdjustAngle(const void* d, void* ctx, float tick, float time)
{
    const int offBaseTickA = 172;
    const int offTimeA     = 176;
    const int offPointA    = 180;
    const int offOriginA   = 192;
    const int offBaseTickB = 204;
    const int offPointB    = 208;

    const char* base = (const char*)ctx;

    const Vector first = part8_PredictAngles(*(const int32_t*)(base + offBaseTickA),
                                             *(const float*)(base + offTimeA),
                                             (const float*)(base + offPointA),
                                             (const float*)(base + offOriginA),
                                             (int)tick, time, true);

    const Vector zero;
    const Vector second = part8_PredictAngles(*(const int32_t*)(base + offBaseTickB), 0.f,
                                              (const float*)(base + offPointB),
                                              (const float*)&zero,
                                              (int)tick, time, true);

    Vector* out = (Vector*)d;

    out->x = first.x + second.x;
    out->y = first.y + second.y;
    out->z = first.z + second.z;

    return out;
}

// ---------------------------------------------------------------------------
//  схема прицеливания
// ---------------------------------------------------------------------------

// Резолвит смещения, на которые опирается выбор записи прицеливания.

void ResetAimState()
{
    static bool resolved = false;

    if (resolved || !g_schemaSystem)
        return;

    resolved = true;

    g_aimOffsetA = SchemaOffset("C_BaseEntity",         "m_nSubclassID");
    g_aimOffsetB = SchemaOffset("CCSWeaponBaseVData",   "m_nDamage");
    g_aimOffsetC = SchemaOffset("CCSWeaponBaseVData",   "m_nNumBullets");
    g_aimOffsetD = SchemaOffset("CCSWeaponBaseVData",   "m_flRange");
    g_aimOffsetE = SchemaOffset("CCSWeaponBaseVData",   "m_flRangeModifier");
    g_aimOffsetF = SchemaOffset("CCSWeaponBaseVData",   "m_flPenetration");
    g_aimOffsetG = SchemaOffset("C_EconEntity",         "m_AttributeManager");
    g_aimOffsetH = SchemaOffset("C_EconItemView",       "m_iItemDefinitionIndex");
    g_aimOffsetI = SchemaOffset("C_AttributeContainer", "m_Item");
    g_aimOffsetJ = SchemaOffset("C_BaseEntity",         "m_iTeamNum");

    // смещения коллизии нужны проходам по стенам, держим их здесь же
    static int collision          = 0;
    static int collisionAttribute = 0;
    static int entityId           = 0;

    collision          = SchemaOffset("C_BaseModelEntity",            "m_Collision");
    collisionAttribute = SchemaOffset("CCollisionProperty",           "m_collisionAttribute");
    entityId           = SchemaOffset("VPhysicsCollisionAttribute_t", "m_nEntityId");
}


// ============================================================================
//  добавлено: part9.cpp
// ============================================================================
// ============================================================================
//  общие помощники
// ============================================================================

static float part9_bitsToFloat(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t part9_floatBits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Пара «тик/доля» упакована в 8 байт: младший dword — тик, старший — float-доля.

static uint64_t part9_makePair(int tick, float frac)
{
    return ((uint64_t)part9_floatBits(frac) << 32) | (uint32_t)tick;
}

static int part9_pairTick(uint64_t pair)
{
    return (int)(uint32_t)pair;
}

static float part9_pairFrac(uint64_t pair)
{
    return part9_bitsToFloat((uint32_t)(pair >> 32));
}

// Максимум по массиву — замена std::max_element с чтением первого элемента.

static float part9_maxFloat(const float* values, int count)
{
    float best = values[0];

    for (int i = 1; i < count; ++i)
    {
        if (best < values[i])
            best = values[i];
    }

    return best;
}

// Приведение доли к диапазону [0, 1) с переносом в целую часть.

static uint64_t part9_packTickFrac(int tick, float frac)
{
    if (frac >= 0.f && frac < 1.f)
        return part9_makePair(tick, frac);

    float integral = 0.f;
    float part = GetTimeFrac(frac, &integral);

    if (part < 0.f)
    {
        part += 1.f;

        if (part < 1.f)
            integral -= 1.f;
        else
            part = 0.f;
    }

    return part9_makePair(tick + (int)integral, part);
}

// Тик точки прицеливания с учётом лаг-компенсации цели.

static int part9_entryTick(void* list, void* entry)
{
    const int offEntryTarget = 16;
    const int offEntryRecord = 24;
    const int offTargetOwner = 8;
    const int offOwnerLag    = 28;   // флаг «цель в лаге»
    const int offOwnerFrac   = 60;
    const int offListLatency = 208;
    const int offListTick    = 24;
    const int offLatencyTick = 72;
    const int offRecordTick  = 16;

    void* record = *(void**)((char*)entry + offEntryRecord);
    void* target = *(void**)((char*)entry + offEntryTarget);
    void* owner  = *(void**)((char*)target + offTargetOwner);

    const int recordTick = *(int*)((char*)record + offRecordTick);

    if (owner && *(uint8_t*)((char*)owner + offOwnerLag))
    {
        void* latency = *(void**)((char*)list + offListLatency);
        return latency ? recordTick - *(int*)((char*)latency + offLatencyTick)
                       : recordTick;
    }

    // Доля тика цели переводится в 64-е доли секунды.
    const float frac = owner ? *(float*)((char*)owner + offOwnerFrac) : 0.f;
    return recordTick - *(int*)((char*)list + offListTick)
         + (int)(frac * 64.f + 0.5f);
}

// Время записи: либо закэшированная пара, либо пара текущего слота,
// поджатая к опорному тику.

static uint64_t part9_resolveRecordTime(void* ctx, void* record)
{
    const int offCached   = 56;
    const int offPair     = 48;
    const int offSlotTick = 348;
    const int offSlotFrac = 360;
    const int offRefTick  = 288;

    if (*(const uint8_t*)((char*)record + offCached))
        return *(const uint64_t*)((char*)record + offPair);

    const uint64_t slot = part9_packTickFrac(*(const int*)((char*)ctx + offSlotTick),
                                             *(const float*)((char*)ctx + offSlotFrac));

    const int   slotTick = part9_pairTick(slot);
    const float slotFrac = part9_pairFrac(slot);

    // Отрицательная доля переносится в предыдущий тик.
    int   tick = slotTick - 3;
    float frac = slotFrac;

    if (frac < 0.f)
    {
        --tick;
        frac += 1.f;
    }

    const int reference = *(const int*)((char*)ctx + offRefTick);

    if (reference < tick || (reference == tick && frac > 0.99999994f))
        return part9_makePair(tick, frac);

    const int next = slotTick + 1;

    if (reference <= next && (reference != next || slotFrac >= 0.99999994f))
        return part9_makePair(reference, 0.99999994f);

    return part9_makePair(next, slotFrac);
}

// Число шагов уточнения по соотношению «урон / время».

static int part9_refineSteps(float score, float time)
{
    int steps = (int)ceilf(score / time);

    if (steps < 1)
        steps = 1;

    if (steps > 6)
        steps = 6;

    return steps;
}

// Подбор сида под разброс. Полный перебор (исходный решатель с шестью
// колбэками) сюда не входит: оставляем только дешёвые проверки запроса.

static bool part9_solveSpread(const void* request, float* out)
{
    const int offMode    = 0;
    const int offWord52  = 52;
    const int offBullets = 60;
    const int offField72 = 72;

    if (!*(const int*)((const char*)request + offMode))
    {
        out[0] = 0.f;
        out[1] = 0.f;
        out[2] = 0.f;
        return true;
    }

    const unsigned bullets = (unsigned)*(const int*)((const char*)request + offBullets);

    // Допустимо от 1 до 16 пуль; для дробовика нужен рабочий паттерн разброса.
    if (bullets - 1 > 15
        || (*(const uint16_t*)((const char*)request + offWord52) == 28
            && *(const float*)((const char*)request + offField72) <= -2147483648.f))
    {
        return false;
    }

    // Сид не найден — вызывающий код откатится на обычный выстрел.
    return false;
}

// ============================================================================
//  видимость точки
//
//  Проверяет, что запись ещё впереди по времени, точка лежит перед
//  направлением трассы и не выходит за допуск по перекрёстному произведению.
// ============================================================================

bool RayTest(void* list, void* entry)
{
    const int offListRayReady = 81;
    const int offEntryRecord  = 24;
    const int offEntryHit     = 40;
    const int offHitTick      = 0;
    const int offHitResolved  = 12;
    const int offRecordDir    = 80;

    if (!*(uint8_t*)((char*)list + offListRayReady))
        return false;

    void* hit = *(void**)((char*)entry + offEntryHit);
    if (!hit || *(uint8_t*)((char*)hit + offHitResolved))
        return false;

    void* record = *(void**)((char*)entry + offEntryRecord);

    // Запись должна быть позже расчётного тика точки.
    if (*(int*)((char*)hit + offHitTick) <= part9_entryTick(list, entry))
        return false;

    const Vector point  = *(const Vector*)entry;
    const Vector origin = *(const Vector*)record;
    const Vector delta  = point - origin;
    const Vector dir    = *(const Vector*)((char*)record + offRecordDir);

    // Точка позади направления трассы — попадания быть не может.
    if (dir.Dot(delta) <= 0.f)
        return false;

    float scale[4];
    scale[0] = 1.f;
    scale[1] = origin.LengthSqr();
    scale[2] = point.LengthSqr();
    scale[3] = delta.LengthSqr();

    const float crossX = dir.y * delta.z - dir.z * delta.y;
    const float crossY = dir.z * delta.x - dir.x * delta.z;
    const float crossZ = dir.x * delta.y - dir.y * delta.x;

    const float crossLenSqr = crossX * crossX + crossY * crossY + crossZ * crossZ;

    // Допуск пропорционален длине направления: чем длиннее луч, тем шире окно.
    return part9_maxFloat(scale, 4) * 2.2737368e-13f * dir.LengthSqr() >= crossLenSqr;
}

// ============================================================================
//  точка прицеливания из истории
//
//  Берёт последнюю записанную пару «тик/доля», проверяет окно вокруг
//  текущего времени и либо отдаёт нижнюю границу, либо сдвигает запись
//  на один шаг сетки (0.01 с).
// ============================================================================

bool FindAimPoint(void* ctx, bool knife, void** out)
{
    const int offTick      = 852;   // обычный путь
    const int offFrac      = 856;
    const int offTickKnife = 860;   // ножевой путь
    const int offFracKnife = 864;
    const int offTimeFrac  = 876;
    const int offCount     = 1836;
    const int offIndex     = 1840;
    const int offHistory   = 1580;  // кольцо из 32 пар {int tick; float frac;}
    const int historySize  = 32;

    const int   baseTick = *(const int*)((char*)ctx + (knife ? offTickKnife : offTick));
    const float baseFrac = *(const float*)((char*)ctx + (knife ? offFracKnife : offFrac));

    // Дробная часть текущего времени сдвигает и тик, и долю.
    float integral = 0.f;
    float frac = GetTimeFrac(*(const float*)((char*)ctx + offTimeFrac), &integral);

    if (frac < 0.f)
    {
        frac += 1.f;

        if (frac < 1.f)
            integral -= 1.f;
        else
            frac = 0.f;
    }

    const float sum     = baseFrac + frac;
    const float fracOut = (sum >= 1.f) ? sum - 1.f : sum;
    const int   tick    = baseTick + (int)integral;

    // Окно: сверху — следующий тик (плюс перенос), снизу — два-три тика назад.
    const int high = tick + ((sum >= 1.f) ? 2 : 1);
    const int low  = tick - ((sum < 1.f) ? 3 : 2);

    const int count = *(const int*)((char*)ctx + offCount);

    uint64_t last = 0;

    if (count > 0)
    {
        int index = (*(const int*)((char*)ctx + offIndex) + count - 1) % historySize;
        index = (index + historySize) % historySize;

        last = part9_makePair(*(const int*)((char*)ctx + offHistory + 8 * index),
                              *(const float*)((char*)ctx + offHistory + 8 * index + 4));
    }

    const int   lastTick = part9_pairTick(last);
    const float lastFrac = part9_pairFrac(last);

    // Последняя запись должна быть строго внутри окна.
    const bool inWindow = (lastTick == high) ? (fracOut > lastFrac)
                                             : (lastTick < high);
    if (!inWindow)
        return false;

    const bool belowLow = (lastTick == low) ? (fracOut > lastFrac)
                                            : (lastTick < low);
    if (belowLow)
    {
        *(int*)out = low;
        *(float*)((char*)out + 4) = fracOut;
        return true;
    }

    // Иначе двигаем запись вперёд на один шаг сетки.
    float deltaFrac = fracOut - lastFrac;
    int   deltaTick = high - lastTick;

    if (deltaFrac < 0.f)
    {
        deltaFrac += 1.f;

        if (deltaFrac >= 1.f)
            deltaFrac = 0.f;
        else
            --deltaTick;
    }

    const float shifted = ((float)deltaTick + deltaFrac) * 0.01f + lastFrac;

    *(uint64_t*)out = part9_packTickFrac(lastTick, shifted);
    return true;
}

// ============================================================================
//  сравнение двух записей прицеливания
//
//  Возвращает true, если первая запись лучше. Порядок проверок повторяет
//  оригинал: сначала шанс/время, затем видимость и метрики блока.
// ============================================================================

bool TryRefine(void* list, void* first, void* second, uint32_t mode, int metricSet)
{
    const int offEntryTime     = 12;
    const int offEntryTarget   = 16;
    const int offEntryRecord   = 24;
    const int offEntryHitPoint = 32;
    const int offEntryHitInfo  = 40;

    // Два набора метрик: A (смещение 52) и B (смещение 68).
    const int offBlockA        = 52;
    const int offBlockB        = 68;
    const int offBlockChance   = 0;
    const int offBlockSecond   = 4;
    const int offBlockThird    = 8;
    const int offBlockFourth   = 12;

    const int offTargetTime    = 108;
    const int offTargetScore   = 104;

    const int offListMinChance = 140;
    const int offListRayReady  = 81;
    const int offListRefine    = 168;
    const int offListThreshold = 188;
    const int offListOrigin    = 84;
    const int offListDir       = 96;

    const int offRecordFlag    = 20;
    const int offRecordTime    = 24;

    const int offHitTick       = 0;
    const int offHitOrigin     = 20;
    const int offHitMins       = 32;
    const int offHitMaxs       = 44;

    if (!first)
        return false;

    if (!second)
        return true;

    const int block = metricSet ? offBlockA : offBlockB;

    const float chanceA = *(float*)((char*)first + block + offBlockChance);
    const float chanceB = *(float*)((char*)second + block + offBlockChance);

    // Запись годится, если её время не позже времени цели и шанс не ниже порога.
    const bool okA = *(float*)((char*)*(void**)((char*)first + offEntryTarget) + offTargetTime)
                         <= *(float*)((char*)first + offEntryTime)
                  && *(float*)((char*)list + offListMinChance) <= chanceA;

    const bool okB = *(float*)((char*)*(void**)((char*)second + offEntryTarget) + offTargetTime)
                         <= *(float*)((char*)second + offEntryTime)
                  && *(float*)((char*)list + offListMinChance) <= chanceB;

    if (okA != okB)
        return okA;

    // Обе годны — при включённом режиме уточнения решает трасса.
    if (okA
        && *(uint8_t*)((char*)list + offListRayReady)
        && *(uint8_t*)((char*)list + offListRefine)
        && *(float*)((char*)list + offListThreshold) == 1.f)
    {
        const bool rayA = RayTest(list, first);
        if (rayA != RayTest(list, second))
            return rayA;
    }

    const float thirdA = *(float*)((char*)first + block + offBlockThird);
    const float thirdB = *(float*)((char*)second + block + offBlockThird);
    const float chanceDelta = fabsf(chanceA - chanceB);

    if (mode == 1)
    {
        // Сначала сравниваем третью метрику, потом шанс.
        if (fabsf(thirdA - thirdB) >= 0.05f)
            return thirdA > thirdB;

        if (chanceDelta >= 0.15f)
            return chanceA > chanceB;
    }
    else
    {
        if (chanceDelta >= 0.15f)
            return chanceA > chanceB;

        if (mode == 2)
        {
            // При равном шансе — кто ближе к направлению взгляда.
            const Vector origin = *(const Vector*)((char*)list + offListOrigin);
            const Vector ref    = *(const Vector*)((char*)list + offListDir);

            const Vector dirA = (*(const Vector*)first - origin).Normalized();
            const Vector dirB = (*(const Vector*)second - origin).Normalized();

            return ref.Dot(dirA) > ref.Dot(dirB);
        }
    }

    void* recordA = *(void**)((char*)first + offEntryRecord);
    void* recordB = *(void**)((char*)second + offEntryRecord);

    // Время записи: предпочитаем более позднюю.
    const float recordTimeA = *(float*)((char*)recordA + offRecordTime);
    const float recordTimeB = *(float*)((char*)recordB + offRecordTime);

    if (recordTimeA != recordTimeB)
        return recordTimeA > recordTimeB;

    // При высоком шансе — меньше шагов уточнения.
    if (chanceA > 0.6f && chanceB > 0.6f)
    {
        const int stepsA = part9_refineSteps(
            *(float*)((char*)*(void**)((char*)first + offEntryTarget) + offTargetScore),
            *(float*)((char*)first + offEntryTime));

        const int stepsB = part9_refineSteps(
            *(float*)((char*)*(void**)((char*)second + offEntryTarget) + offTargetScore),
            *(float*)((char*)second + offEntryTime));

        if (stepsA != stepsB)
            return stepsA < stepsB;
    }

    if (chanceDelta >= 0.07f)
        return chanceA > chanceB;

    const float secondA = *(float*)((char*)first + block + offBlockSecond);
    const float secondB = *(float*)((char*)second + block + offBlockSecond);

    if (fabsf(secondA - secondB) >= 0.1f && (secondA > 0.4f || secondB > 0.4f))
        return secondA > secondB;

    if (chanceA != chanceB)
        return chanceA > chanceB;

    // Насколько точка отстаёт от тика попадания: меньше — лучше.
    const int hitDeltaA = *(int*)((char*)*(void**)((char*)first + offEntryHitInfo) + offHitTick)
                        - part9_entryTick(list, first);
    const int hitDeltaB = *(int*)((char*)*(void**)((char*)second + offEntryHitInfo) + offHitTick)
                        - part9_entryTick(list, second);

    if ((hitDeltaA > 0 || hitDeltaB > 0) && hitDeltaA != hitDeltaB)
        return hitDeltaA < hitDeltaB;

    if (secondA == secondB || (secondB <= 0.15f && secondA <= 0.15f))
    {
        // Флаг записи: непустой считается лучше.
        const uint8_t flagA = *(uint8_t*)((char*)recordA + offRecordFlag);
        const uint8_t flagB = *(uint8_t*)((char*)recordB + offRecordFlag);

        if (flagA != flagB)
            return flagA != 0;

        const float fourthA = *(float*)((char*)first + block + offBlockFourth);
        const float fourthB = *(float*)((char*)second + block + offBlockFourth);

        if (fabsf(fourthA - fourthB) <= 2.f)
        {
            if (fabsf(thirdA - thirdB) >= 0.05f)
                return thirdA > thirdB;

            Vector pointA;
            Vector pointB;

            if (chanceA >= 0.05f)
            {
                void* hitPointA = *(void**)((char*)first + offEntryHitPoint);
                void* hitPointB = *(void**)((char*)second + offEntryHitPoint);

                pointA = hitPointA ? *(const Vector*)hitPointA : Vector();
                pointB = hitPointB ? *(const Vector*)hitPointB : Vector();
            }
            else
            {
                // Центр хитбокса: origin + (mins + maxs) / 2.
                void* hitA = *(void**)((char*)first + offEntryHitInfo);
                void* hitB = *(void**)((char*)second + offEntryHitInfo);

                pointA = *(const Vector*)((char*)hitA + offHitOrigin)
                       + (*(const Vector*)((char*)hitA + offHitMins)
                        + *(const Vector*)((char*)hitA + offHitMaxs)) * 0.5f;

                pointB = *(const Vector*)((char*)hitB + offHitOrigin)
                       + (*(const Vector*)((char*)hitB + offHitMins)
                        + *(const Vector*)((char*)hitB + offHitMaxs)) * 0.5f;
            }

            const float distA = (*(const Vector*)first - pointA).LengthSqr();
            const float distB = (*(const Vector*)second - pointB).LengthSqr();

            if (fabsf(distA - distB) > 0.1f)
                return distA < distB;
        }

        return fourthA > fourthB;
    }

    return secondA > secondB;
}

// ============================================================================
//  применение промаха
//
//  Собирает запрос на подбор сида под разброс и, если сид найден, кладёт
//  готовые углы в hitInfo записи (0..8 — углы, 12 — признак «углы есть»).
// ============================================================================

bool ApplyAimMiss(void* shot, int mode, void* list, void* entry, int flag)
{
    const int offEntryTarget = 16;
    const int offEntryRecord = 24;
    const int offEntryHit    = 40;

    const int offShotFlagA   = 0;
    const int offShotFlagB   = 1;
    const int offShotWord    = 1992;
    const int offShotIndex   = 1952;
    const int offShotField   = 1940;
    const int offShotWeapon  = 1984;
    const int offShotSpread  = 1896;

    const int offHitOrigin   = 20;
    const int offHitMins     = 32;
    const int offHitMaxs     = 44;
    const int offHitResolved = 12;

    (void)list;

    if (!mode)
        return true;

    // Ленивый резолв схемы — как в оригинале.
    static bool s_offsetsReady = false;
    static int  s_absScale = 0;
    static int  s_spreadOffset = 0;

    if (!s_offsetsReady && g_schemaSystem)
    {
        s_offsetsReady = true;

        g_off_m_nNumBullets = GetSchemaOffset("CCSWeaponBaseVData", "m_nNumBullets");
        off_m_pGameSceneNode = GetSchemaOffset("C_BaseEntity", "m_pGameSceneNode");
        s_absScale          = GetSchemaOffset("CGameSceneNode", "m_flAbsScale");
        s_spreadOffset      = GetSchemaOffset("CCSWeaponBaseVData", "m_flSpread");
    }

    void* record = *(void**)((char*)entry + offEntryRecord);
    if (!record)
        return false;

    void* target = *(void**)((char*)entry + offEntryTarget);
    if (!target)
        return false;

    void* hit = *(void**)((char*)entry + offEntryHit);
    if (!hit)
        return false;

    // Запрос на подбор сида. Раскладка повторяет оригинальную (91 байт).
    struct SpreadRequest
    {
        int      mode;         // +0
        uint8_t  flagA;        // +4
        uint8_t  flagB;        // +5
        uint8_t  flagC;        // +6
        uint8_t  pad0;
        Vector   point;        // +8
        Vector   origin;       // +20
        int      tick;         // +32
        Vector   center;       // +36
        float    radius;       // +48
        uint16_t word52;       // +52
        uint16_t pad1;
        int      field56;      // +56
        int      bullets;      // +60
        float    spread;       // +64
        float    spread2;      // +68
        int      field72;      // +72
        Vector   angles;       // +76
        uint8_t  flag88;       // +88
        uint8_t  flag89;       // +89
        uint8_t  flag90;       // +90
    };

    static_assert(offsetof(SpreadRequest, mode)    == 0,  "mode");
    static_assert(offsetof(SpreadRequest, point)   == 8,  "point");
    static_assert(offsetof(SpreadRequest, origin)  == 20, "origin");
    static_assert(offsetof(SpreadRequest, tick)    == 32, "tick");
    static_assert(offsetof(SpreadRequest, center)  == 36, "center");
    static_assert(offsetof(SpreadRequest, radius)  == 48, "radius");
    static_assert(offsetof(SpreadRequest, word52)  == 52, "word52");
    static_assert(offsetof(SpreadRequest, field56) == 56, "field56");
    static_assert(offsetof(SpreadRequest, bullets) == 60, "bullets");
    static_assert(offsetof(SpreadRequest, spread)  == 64, "spread");
    static_assert(offsetof(SpreadRequest, spread2) == 68, "spread2");
    static_assert(offsetof(SpreadRequest, field72) == 72, "field72");
    static_assert(offsetof(SpreadRequest, angles)  == 76, "angles");
    static_assert(offsetof(SpreadRequest, flag90)  == 90, "flag90");

    // Время записи и доля для AdjustAngle.
    uint64_t timePair = part9_resolveRecordTime((char*)shot + 16, record);
    const int   tick  = part9_pairTick(timePair);
    const float frac  = part9_pairFrac(timePair);

    void* weaponData = *(void**)((char*)shot + offShotWeapon);

    int bullets = 0;
    if (weaponData && g_off_m_nNumBullets
        && IsValidAddr((uintptr_t)((char*)weaponData + g_off_m_nNumBullets)))
    {
        bullets = ReadInt((char*)weaponData + g_off_m_nNumBullets);
    }

    const float spread = GetShotSpread((char*)shot + offShotSpread);
    ComputeShotSpread();

    // Разброс зависит от индекса выстрела в очереди (0 или 1).
    int index = *(int*)((char*)shot + offShotIndex);
    if (index > 1)
        index = 0;

    float spread2 = 0.f;
    if (weaponData && s_spreadOffset
        && IsValidAddr((uintptr_t)((char*)weaponData + s_spreadOffset + 4 * index)))
    {
        spread2 = ReadFloat((char*)weaponData + s_spreadOffset + 4 * index);
    }

    // Углы точки с учётом текущего времени.
    const float* angles = (const float*)AdjustAngle(&timePair, (char*)shot + 16,
                                                   (float)tick, frac);

    // Центр и радиус хитбокса цели; масштаб берётся у сцены сущности.
    float scale = 1.f;
    if (off_m_pGameSceneNode && s_absScale)
    {
        void* entity = ReadPtr(target);
        if (entity)
        {
            void* node = ReadPtr((char*)entity + off_m_pGameSceneNode);
            if (node && IsValidAddr((uintptr_t)((char*)node + s_absScale)))
                scale = ReadFloat((char*)node + s_absScale);
        }
    }

    const Vector origin = *(const Vector*)((char*)hit + offHitOrigin);
    const Vector mins   = *(const Vector*)((char*)hit + offHitMins);
    const Vector maxs   = *(const Vector*)((char*)hit + offHitMaxs);

    SpreadRequest request;
    memset(&request, 0, sizeof(request));

    request.mode    = mode;
    request.flagA   = *(uint8_t*)((char*)shot + offShotFlagA);
    request.flagB   = *(uint8_t*)((char*)shot + offShotFlagB);
    request.flagC   = (uint8_t)flag;
    request.point   = *(const Vector*)entry;
    request.origin  = *(const Vector*)record;
    request.tick    = tick;
    request.center  = origin + (mins + maxs) * 0.5f * scale;
    request.radius  = (maxs - mins).Length() * 0.5f;
    request.word52  = *(uint16_t*)((char*)shot + offShotWord);
    request.field56 = *(int*)((char*)shot + offShotIndex);
    request.bullets = bullets;
    request.spread  = spread;
    request.spread2 = spread2;
    request.field72 = *(int*)((char*)shot + offShotField);
    request.angles  = Vector(angles[0], angles[1], angles[2]);

    // Отладочные cvar'ы движка (weapon_debug_*) в реконструкции выключены.
    request.flag88 = 0;
    request.flag89 = 0;
    request.flag90 = 0;

    float result[3] = { 0.f, 0.f, 0.f };
    if (!part9_solveSpread(&request, result))
        return false;

    // Пишем результат прямо в hitInfo записи: так его увидит вызывающий код.
    float* out = (float*)hit;
    out[0] = result[0];
    out[1] = result[1];
    out[2] = result[2];
    *(uint8_t*)((char*)hit + offHitResolved) = 1;

    return true;
}


// ============================================================================
//  добавлено: part10.cpp
// ============================================================================


#include <cstdio>

// ============================================================================
//  скан-тик и отладочный сброс состояния
// ============================================================================

namespace
{

// Точка, которую скан-тик дописывает в запрос выстрела. Формат совпадает
// с обычной точкой прохода: вес, набор флагов и признак готовности.
struct part10_ScanPoint
{
    float    weight;    // +0  вес точки
    uint32_t pad0;      // +4
    uint64_t flags;     // +8  флаги прохода (1 / 0x800)
    uint8_t  flag16;    // +16 дополнительный признак
    char     pad1[7];
    char     tail[48];
};

static_assert(sizeof(part10_ScanPoint) == 72, "scan point layout");

// Сравнение отметки времени с опорной парой «тик + доля».

bool part10_Before(int tick, int refTick, float refFrac)
{
    if (tick == refTick)
        return refFrac > 0.f;

    return tick < refTick;
}

// Точка попадает в окно, если тик совпал с опорным или опорный тик
// на единицу меньше при ненулевой доле.

bool part10_InWindow(int tick, int refTick, float refFrac)
{
    const bool before = part10_Before(tick, refTick, refFrac);

    return !before
        && (tick == refTick || (refTick == tick - 1 && refFrac > 0.f));
}

// Дописывает точку в список запроса (тройка begin/end/cap по 72 байта).

void part10_AppendScanPoint(void* shot, float weight, uint64_t flags, uint8_t flag16)
{
    const int offBegin = 64;
    const int offEnd   = 72;
    const int offCap   = 80;
    const int step     = (int)sizeof(part10_ScanPoint);

    char* base = *(char**)((char*)shot + offBegin);
    char* end  = *(char**)((char*)shot + offEnd);
    char* cap  = *(char**)((char*)shot + offCap);

    if (end == cap)
    {
        const size_t count    = (size_t)(end - base) / step;
        const size_t capacity = (size_t)(cap - base) / step;

        size_t grown = capacity + capacity / 2;
        if (grown <= count)
            grown = count + 1;

        char* memory = (char*)realloc(base, grown * step);
        if (!memory)
            return;

        base = memory;
        end  = memory + count * step;

        *(char**)((char*)shot + offBegin) = memory;
        *(char**)((char*)shot + offCap)   = memory + grown * step;
    }

    part10_ScanPoint point{};
    point.weight = weight;
    point.flags  = flags;
    point.flag16 = flag16;

    memcpy(end, &point, step);
    *(char**)((char*)shot + offEnd) = end + step;
}

} // namespace

// ============================================================================
//  OnScanTick
//
//  Вызывается решателем на каждом шаге прохода. Дописывает в запрос
//  служебные точки остановки и выставляет биты «стрелять / не стрелять»
//  в зависимости от того, попал ли текущий тик в окно упреждения.
// ============================================================================

void OnScanTick(int mode, void* ctxPtr, void* shotPtr)
{
    if (mode == 2)
        return;

    char* ctx  = (char*)ctxPtr;
    char* shot = (char*)shotPtr;

    if (!ctx || !shot)
        return;

    const int offFlagsA = 8;    // кнопки/флаги текущего шага
    const int offFlagsB = 16;   // флаги второго прохода
    const int offFlagsC = 24;   // флаги третьего прохода

    const uint64_t flagsA = *(uint64_t*)(shot + offFlagsA);
    const uint64_t flagsB = *(uint64_t*)(shot + offFlagsB);
    const uint64_t flagsC = *(uint64_t*)(shot + offFlagsC);

    // Уже стреляем или включён ручной стоп — точку не трогаем.
    if (((flagsC | flagsA) & 0x2001) != 0
        || *(int*)(ctx + 884) <= 0
        || *(uint8_t*)(ctx + 1940))
    {
        return;
    }

    const int tick = *(int*)(ctx + 348);

    // Снимаем биты остановки с обоих наборов.
    const uint64_t clearedA = flagsA & ~0x2827ull;
    *(uint64_t*)(shot + offFlagsA) = clearedA;
    *(uint64_t*)(shot + offFlagsC) = flagsC & ~0x800ull;

    // Быстрый путь: окно ещё не подошло — просто помечаем «ждать».
    if (!*(uint8_t*)(ctx + 894))
    {
        if (!part10_Before(tick, *(int*)(ctx + 852), *(float*)(ctx + 856)))
            *(uint64_t*)(shot + offFlagsA) = clearedA | 1;

        return;
    }

    // ---------------------------------------------------- раннее окно
    const int   earlyTick = *(int*)(ctx + 868);
    const float earlyFrac = *(float*)(ctx + 872);

    if (part10_InWindow(tick, earlyTick, earlyFrac))
    {
        part10_AppendScanPoint(shot, 0.f, 0x800, 1);

        *(uint64_t*)(shot + offFlagsA) = (clearedA & ~1ull) | 0x800;

        // Доля времени округляется вниз до шага 1/64: так же, как в
        // оригинале, где из неё вычитается 0.015625.
        float weight;

        if (earlyTick == tick && earlyFrac == 0.f)
        {
            weight = 1.f;
        }
        else
        {
            const float truncated = (float)(earlyFrac + 131072.0) - 131072.0;

            weight = (earlyFrac >= truncated)
                   ? truncated
                   : fmaxf(0.f, truncated - 0.015625f);
        }

        part10_AppendScanPoint(shot, weight, 1, 0);
        return;
    }

    // ---------------------------------------------------- позднее окно
    const int   lateTick = *(int*)(ctx + 860);
    const float lateFrac = *(float*)(ctx + 864);

    if (part10_InWindow(tick, lateTick, lateFrac))
    {
        part10_AppendScanPoint(shot, 0.f, 0x800, 0);

        *(uint64_t*)(shot + offFlagsA) = (clearedA & ~0x800ull) | 1;

        // Две точки: 63/64 и полный вес — вторая закрывает окно.
        part10_AppendScanPoint(shot, 0.984375f, 1, 1);
        part10_AppendScanPoint(shot, 1.f,        1, 1);
        return;
    }

    // ---------------------------------------------------- окна впереди
    if (part10_Before(tick, earlyTick, earlyFrac))
    {
        *(uint64_t*)(shot + offFlagsA) = clearedA | 1;
        *(uint64_t*)(shot + offFlagsB) = flagsB | 1;
        *(uint64_t*)(shot + offFlagsC) = (flagsC & ~0x800ull) | 1;
        return;
    }

    if (part10_Before(tick, lateTick, lateFrac))
    {
        *(uint64_t*)(shot + offFlagsA) = flagsA | 0x800;
        *(uint64_t*)(shot + offFlagsB) = flagsB | 0x800;
        *(uint64_t*)(shot + offFlagsC) = (flagsC & ~0x800ull) | 0x800;
    }
}

// ============================================================================
//  журнал
// ============================================================================

// Единая точка записи строки rage-лога. В оригинале строка уходит в
// лог-поток под SRW-локом; здесь — в тот же файл, что и у остальных
// отладочных сообщений.

void LogRageLine(const char* line)
{
    if (!line)
        return;

    FILE* file = fopen("wl_rage.log", "a");
    if (!file)
        return;

    fputs(line, file);
    fputc('\n', file);
    fclose(file);
}

// ============================================================================
//  хитчанс и force shot
//
//  Это не Rage-модуль, а просчёт, которым он пользуется: 64 сида разброса,
//  доля пробивающих, доворот точки по среднему направлению попаданий.
//  Держим здесь, чтобы вся цепочка лежала в одном файле.
// ============================================================================

// Сид разброса — пара смещений по осям базиса.

struct SpreadSeed
{
    float x;
    float y;
};

// Сетка сидов: 64 пары по спирали, одна на все выстрелы.

static const SpreadSeed kSpreadPattern[64] =
{
    {  0.125000f,  0.000000f }, { -0.130350f,  0.119411f },
    {  0.018928f, -0.215677f }, {  0.152110f,  0.198400f },
    { -0.275236f, -0.048685f }, {  0.258346f, -0.164339f },
    { -0.085856f,  0.319380f }, { -0.162956f, -0.313760f },
    {  0.352246f,  0.128639f }, { -0.365380f,  0.150824f },
    {  0.175717f, -0.375498f }, {  0.129594f,  0.413165f },
    { -0.389946f, -0.225981f }, {  0.456798f, -0.100427f },
    { -0.278433f,  0.396043f }, { -0.064256f, -0.495854f },
    {  0.394092f,  0.332140f }, { -0.529877f,  0.021913f },
    {  0.386214f, -0.384335f }, { -0.025821f,  0.558420f },
    { -0.367013f, -0.439803f }, {  0.581066f,  0.078181f },
    { -0.492087f,  0.342382f }, {  0.134404f, -0.597441f },
    {  0.310740f,  0.542278f }, { -0.607226f, -0.193719f },
    {  0.589627f, -0.272425f }, { -0.255353f,  0.610160f },
    { -0.227830f, -0.633418f }, {  0.606049f,  0.318520f },
    { -0.672982f,  0.177398f }, {  0.382428f, -0.594768f },
    {  0.121626f,  0.707695f }, { -0.576262f, -0.446287f },
    {  0.736985f, -0.061060f }, { -0.509308f,  0.550550f },
    {  0.003707f, -0.760336f }, {  0.517724f,  0.570712f },
    { -0.777294f, -0.072038f }, {  0.629735f, -0.477948f },
    { -0.143255f,  0.787466f }, { -0.431461f, -0.685632f },
    {  0.790530f,  0.216650f }, { -0.737688f,  0.378571f },
    {  0.291485f, -0.786233f }, {  0.319654f,  0.785221f },
    { -0.774394f, -0.366999f }, {  0.827585f, -0.255154f },
    { -0.442413f,  0.754914f }, { -0.185580f, -0.864182f },
    {  0.727758f,  0.516956f }, { -0.894467f,  0.111483f },
    {  0.589838f, -0.692976f }, {  0.033482f,  0.917948f },
    { -0.650696f, -0.660280f }, {  0.934194f,  0.047759f },
    { -0.727517f,  0.601119f }, {  0.131537f, -0.942840f },
    {  0.544522f,  0.790804f }, { -0.943590f, -0.217112f },
    {  0.849422f, -0.481256f }, { -0.303713f,  0.936220f },
    { -0.411740f, -0.902688f }, {  0.920581f,  0.390551f },
};

// Данные одной трассировки: сначала две маски по 64 бита, за ними урон.

struct HitchanceData
{
    uint32_t lethalMask[2];     // +0x00
    uint32_t playerMask[2];     // +0x08
    float    damage[64];        // +0x10
    uint32_t hitCount;
};

// Контекст просчёта: режим маски попаданий и сиды текущего масштаба.

struct HitchanceContext
{
    char         pad0[0xC8];
    int          hitMaskMode;       // +0xC8
    char         pad1[0x7C];
    SpreadSeed   seeds[64];         // +0x148
};

// Цель, по которой считается попадание.

struct HitchanceTarget
{
    char     pad0[0x44];
    int      skipPlayerMask;        // +0x44
};

// Настройки просчёта, привязанные к точке.

struct HitchanceSettings
{
    char              pad0[0x08];
    HitchanceTarget*  target;           // +0x08
    char              pad1[0x58];
    float             minimumDamage;    // +0x68
    float             targetHealth;     // +0x6C
};

// Точка прицела.

struct HitchancePoint
{
    Vector             position;        // +0x00
    char               pad0[0x04];
    HitchanceSettings* settings;        // +0x10
    Vector*            shootPosition;   // +0x18
};

// ---------------------------------------------------------------------------
//  Движок трасс.
//
//  В оригинале это вызов игрового батча; здесь — два обратных вызова,
//  которые подставляет вызывающий.
// ---------------------------------------------------------------------------

struct HitchanceHit
{
    float damage;
    bool  hitTarget;
    bool  hitPlayer;
};

typedef bool  (*TraceSeedFn)(void* user, const Vector& direction, HitchanceHit* out);
typedef float (*TraceDamageFn)(void* user, const Vector& point);

static TraceSeedFn   g_traceSeed;
static TraceDamageFn g_traceDamage;
static void*         g_traceUser;

void SetHitchanceEngine(TraceSeedFn seed, TraceDamageFn damage, void* user)
{
    g_traceSeed   = seed;
    g_traceDamage = damage;
    g_traceUser   = user;
}

// ---------------------------------------------------------------------------
//  Вспомогательное.
// ---------------------------------------------------------------------------

static Vector NormalizeSafe(const Vector& value)
{
    const float lengthSqr = value.LengthSqr();

    if (lengthSqr <= 0.f)
        return Vector();

    const float length = sqrtf(lengthSqr);

    // Слишком короткий вектор не трогаем: деление дало бы мусор.
    if (length <= 1e-5f)
        return value;

    return value * (1.f / length);
}

static uint32_t CountSetBits(uint32_t value)
{
    uint32_t count = 0;

    while (value != 0)
    {
        value &= value - 1;
        ++count;
    }

    return count;
}

// Сиды на текущий масштаб разброса.

static void BuildSpreadSeeds(HitchanceContext* context, float scale)
{
    for (int seed = 0; seed < 64; ++seed)
    {
        context->seeds[seed].x = kSpreadPattern[seed].x * scale;
        context->seeds[seed].y = kSpreadPattern[seed].y * scale;
    }
}

// ---------------------------------------------------------------------------
//  Базис выстрела: направление от точки выстрела к точке прицела и две
//  перпендикулярные оси. При почти вертикальном направлении горизонтальная
//  ось вырождается, поэтому подставляется фиксированная.
// ---------------------------------------------------------------------------

static void BuildBasis(const HitchancePoint* point, float scale,
                       Vector& forward, Vector& right, Vector& up)
{
    const Vector delta = point->position - *point->shootPosition;

    forward = NormalizeSafe(delta);

    if (fabsf(forward.x) < 0.001f && fabsf(forward.y) < 0.001f)
        right = Vector(0.f, 1.f, 0.f);
    else
        right = NormalizeSafe(Vector(-forward.y, forward.x, 0.f));

    up = right.Cross(forward);

    forward = forward * scale;
    right   = right * scale;
    up      = up * scale;
}

// ---------------------------------------------------------------------------
//  Батч по сидам.
// ---------------------------------------------------------------------------

static void RunTraceBatch(const Vector& forward, const Vector& right, const Vector& up,
                          const SpreadSeed* seeds, int count,
                          HitchanceData* data)
{
    data->lethalMask[0] = data->lethalMask[1] = 0;
    data->playerMask[0] = data->playerMask[1] = 0;

    for (int seed = 0; seed < count; ++seed)
    {
        data->damage[seed] = 0.f;

        if (!g_traceSeed)
            continue;

        const SpreadSeed spread = seeds[seed];

        const Vector direction = NormalizeSafe(forward
                                               + right * spread.x
                                               + up * spread.y);

        HitchanceHit hit = {};

        if (!g_traceSeed(g_traceUser, direction, &hit))
            continue;

        data->damage[seed] = hit.damage;

        if (hit.hitTarget)
            data->lethalMask[seed >> 5] |= (uint32_t)1 << (seed & 31);

        if (hit.hitPlayer)
            data->playerMask[seed >> 5] |= (uint32_t)1 << (seed & 31);
    }
}

// ---------------------------------------------------------------------------
//  Доля сидов, пробивших минимальный урон.
// ---------------------------------------------------------------------------

static float CalculateHitchance(const HitchanceData& data, float minimumDamage)
{
    uint32_t valid = 0;

    for (int seed = 0; seed < 64; ++seed)
    {
        if (data.damage[seed] >= minimumDamage)
            ++valid;
    }

    return (float)valid / 64.f;
}

// ---------------------------------------------------------------------------
//  Один просчёт.
//
//  Возвращает true, когда пробивают все сиды. Иначе точка сдвигается на
//  среднее направление попаданий и функция возвращает false.
// ---------------------------------------------------------------------------

static bool HitChance(HitchanceContext* context, HitchancePoint* point, float scale)
{
    HitchanceData data = {};

    BuildSpreadSeeds(context, scale);

    Vector forward, right, up;
    BuildBasis(point, scale, forward, right, up);

    RunTraceBatch(forward, right, up, context->seeds, 64, &data);

    data.hitCount = CountSetBits(data.lethalMask[0]) + CountSetBits(data.lethalMask[1]);

    if (data.hitCount == 0)
        return false;

    const float hitchance = CalculateHitchance(data, point->settings->minimumDamage);

    if (hitchance >= 1.f)
        return true;

    const uint32_t* mask = data.lethalMask;

    // Режим маски игрока: маска берётся, только когда цель для неё годится.
    if (context->hitMaskMode == 1)
    {
        const HitchanceTarget* target = point->settings->target;

        if (!target || target->skipPlayerMask == 0)
            mask = data.playerMask;
        else
            return false;
    }

    // Предел урона растёт вместе с отношением требуемого урона к здоровью.
    const float minimumDamage = point->settings->minimumDamage;
    const float targetHealth  = point->settings->targetHealth;
    const float ratio         = minimumDamage / targetHealth;
    const float damageLimit   = minimumDamage * (1.f + fmaxf(0.f, ratio - 1.f) * 2.5f);

    Vector directionSum;
    float  damageSum = 0.f;

    for (int seed = 0; seed < 64; ++seed)
    {
        if ((mask[seed >> 5] & ((uint32_t)1 << (seed & 31))) == 0)
            continue;

        const SpreadSeed spread = context->seeds[seed];

        const Vector direction = NormalizeSafe(forward
                                               + right * spread.x
                                               + up * spread.y);

        const float seedDamage = fminf(damageLimit, data.damage[seed]);

        directionSum = directionSum + direction * seedDamage;
        damageSum += seedDamage;
    }

    if (damageSum <= 0.f)
        return false;

    const Vector average = directionSum * (1.f / damageSum);

    const Vector delta = point->position - *point->shootPosition;

    // Точка переставляется на луч среднего направления.
    const float along = delta.Dot(average);

    point->position = *point->shootPosition + average * along;

    return false;
}

// ---------------------------------------------------------------------------
//  Приёмка сдвинутой точки.
//
//  Углы сравниваются лесенкой допусков: решает первая ось, вышедшая за порог.
// ---------------------------------------------------------------------------

static bool AngleIsBetter(const Vector& candidate, const Vector& current)
{
    if (fabsf(candidate.x - current.x) > 0.15f) return candidate.x > current.x;
    if (fabsf(candidate.y - current.y) > 0.12f) return candidate.y > current.y;
    if (fabsf(candidate.x - current.x) > 0.08f) return candidate.x > current.x;
    if (fabsf(candidate.y - current.y) > 0.05f) return candidate.y > current.y;
    if (fabsf(candidate.x - current.x) > 0.05f) return candidate.x > current.x;

    return candidate.z > current.z;
}

static bool ConfirmShiftedPoint(HitchancePoint* point, const HitchancePoint& original)
{
    if (point->position.x == original.position.x &&
        point->position.y == original.position.y &&
        point->position.z == original.position.z)
    {
        return false;
    }

    const float damage = g_traceDamage ? g_traceDamage(g_traceUser, point->position) : 0.f;

    if (damage <= 0.f || damage > point->settings->targetHealth)
    {
        point->position = original.position;
        return false;
    }

    if (!AngleIsBetter(point->position, original.position))
    {
        point->position = original.position;
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
//  force shot: до трёх проходов с уменьшающимся разбросом.
//
//  Число попыток ограничено тремя; на одной попытке разброс полный, дальше
//  масштаб едет от единицы до minimumScale равными шагами.
// ---------------------------------------------------------------------------

bool ForceShot(HitchanceContext* context, HitchancePoint* point,
               int iterations, float minimumScale)
{
    const HitchancePoint original = *point;

    bool confirmed = false;

    if (iterations < 1) iterations = 1;
    if (iterations > 3) iterations = 3;

    if (iterations == 1)
    {
        confirmed = HitChance(context, point, 1.f);
    }
    else
    {
        const float divisor = (float)(iterations - 1);
        const float range   = 1.f - minimumScale;

        for (int pass = 0; pass < iterations; ++pass)
        {
            const float scale = 1.f - range * (float)pass / divisor;

            if (HitChance(context, point, scale))
            {
                confirmed = true;
                break;
            }
        }
    }

    if (confirmed)
        return true;

    return ConfirmShiftedPoint(point, original);
}

// ============================================================================
//  заполнитель результата команды
//
//  Пишет байты +3..+7 и числовые поля, которые затем читает снимок настроек.
//  Порядок в rage-корне: сначала заполнитель, потом снимок.
// ============================================================================

void FillCommandResult(const void* angles)
{
    if (!g_result)
        return;

    const char* view = (const char*)angles;

    // +6 — глобальный флаг допуска: второй бит слова кнопок.
    g_result->gateFlag = (uint8_t)((g_cmdFlags & 2) != 0);

    // +7 — «оба условия»; ниже выставляется по списку записей.
    g_result->bothFlag = 0;

    // +5 — флаг слота из команды.
    g_result->slotFlag = (uint8_t)(view ? (*(const uint8_t*)(view + 40) & 1) : 0);

    // Список записей по 72 байта: вид в +8, признак в +16.
    // Бит 0 — запись вида 4 без признака, бит 1 — запись вида 2 с признаком.
    if (g_local && (g_local->flags & 3) == 2)
    {
        uint32_t mask = 0;

        for (const char* it = (const char*)g_shotsBegin; it != (const char*)g_shotsEnd; it += 0x48)
        {
            const uint64_t kind = *(const uint64_t*)(it + 8);
            const uint8_t  flag = *(const uint8_t*)(it + 16);

            if (kind == 4)
            {
                mask |= 1u;

                if (flag)
                    mask &= ~1u;
            }
            else if (kind == 2)
            {
                mask |= 2u;

                if (!flag)
                    mask &= ~2u;
            }

            if (mask == 3u)
                break;
        }

        g_result->bothFlag = (uint8_t)(mask == 3u);
    }

    // +0x18 — шаг уточнения: по умолчанию 1/64, хук может подменить.
    {
        float refine = 0.015625f;

        g_result->refineStep = (g_refineHook && g_refineHook(&refine))
                                   ? refine
                                   : 0.015625f;
    }

    // +0x1C/+0x20/+0x24 — углы команды.
    g_result->angleX = view ? *(const float*)(view + 0) : 0.f;
    g_result->angleY = view ? *(const float*)(view + 4) : 0.f;
    g_result->angleZ = view ? *(const float*)(view + 8) : 0.f;

    // +0x28/+0x2C/+0x30 — направление из этих углов.
    {
        const float degToRad = 0.017453292f;
        const float pitch    = g_result->angleX * degToRad;
        const float yaw      = g_result->angleY * degToRad;
        const float cp = cosf(pitch), sp = sinf(pitch);
        const float cy = cosf(yaw),   sy = sinf(yaw);

        g_result->forwardX = cp * cy;
        g_result->forwardY = cp * sy;
        g_result->forwardZ = -sp;
    }

    if (!g_local)
        return;

    const char* local = (const char*)g_local;

    // +0x34 — начало луча: origin + viewOffset по x и y.
    g_result->originXY[0] = *(const float*)(local + 0x00) + *(const float*)(local + 0x24);
    g_result->originXY[1] = *(const float*)(local + 0x04) + *(const float*)(local + 0x28);

    // +0x3C — сумма двух локальных полей.
    g_result->height = *(const float*)(local + 0x2C) + *(const float*)(local + 0x08);

    // +0x40 — origin как он есть.
    g_result->baseXY[0] = *(const float*)(local + 0x00);
    g_result->baseXY[1] = *(const float*)(local + 0x04);

    // +0x48 — третья компонента origin.
    g_result->field48 = *(const int32_t*)(local + 0x08);

    // +0x10/+0x14 — дистанция из хука и она же, умноженная на 64.
    {
        const float distance = g_hookDistance;

        g_result->field10 = *(const int32_t*)&distance;
        g_result->field14 = (int32_t)(distance * 64.f + 0.5f);
    }

    // +3/+4 — флаги сравнения текущего времени с парами локальной структуры.
    {
        const TickPair now = part1_Normalize(g_local->tick, 0.f);

        const bool laterA = (now.tick == g_local->tickA)
                                ? (g_local->fracA > now.frac)
                                : (now.tick < g_local->tickA);

        const bool laterB = (now.tick == g_local->tickB)
                                ? (g_local->fracB > now.frac)
                                : (now.tick < g_local->tickB);

        g_result->compareA = (uint8_t)!laterA;
        g_result->compareB = (uint8_t)!laterB;

        // Для оружия с идентификатором 64 третья пара может снять первый флаг.
        if (g_local->weaponId == 64)
        {
            const bool laterC = (now.tick == g_local->tickC)
                                    ? (g_local->fracC > now.frac)
                                    : (now.tick < g_local->tickC);

            if (laterC)
                g_result->compareA = 0;
        }
    }

    // +1/+2 — слово «можно стрелять».
    g_result->canShoot = 0;
    g_result->wordHi   = 0;

    // Три локальных признака снимают проверку целиком.
    if (*(const uint8_t*)(local + 0x16C) != 0 ||
        (g_local->flags & 0x20) != 0 ||
        *(const uint8_t*)(local + 0x71) != 0)
    {
        return;
    }

    const void* weaponData = g_local->weaponData;

    int type = 0;

    if (weaponData && off_m_WeaponType
        && Readable((const char*)weaponData + off_m_WeaponType))
    {
        type = ReadInt((const char*)weaponData + off_m_WeaponType);
    }

    // Быстрый путь: тип оружия известен и это не нож — стрелять можно сразу.
    if (type != 0 && g_local->weaponId != 0x1F)
    {
        g_result->canShoot = 1;
        g_result->wordHi   = 1;
        return;
    }

    int category = -1;

    if (weaponData && off_m_WeaponCategory
        && Readable((const char*)weaponData + off_m_WeaponCategory))
    {
        category = ReadInt((const char*)weaponData + off_m_WeaponCategory);
    }

    bool canShoot = false;

    if (category == 1)
    {
        canShoot = false;
    }
    else if (category == 0)
    {
        canShoot = (g_local->weaponId == 0x1F) && g_local->clip > 0;
    }
    else
    {
        canShoot = g_local->clip > 0;
    }

    g_result->canShoot = (uint8_t)canShoot;
}
