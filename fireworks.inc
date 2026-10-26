/*
 * fireworks.inc - advanced fireworks for SA-MP gamemodes
 * ----------------------------------------------------------------------------
 * Place or throw fireworks, pick a shell type and colours, light the fuse and
 * watch them burst into coloured shapes in the sky. Multi-shot cakes, ground
 * fountains and an automatic firework show.
 *
 * Plain SA-MP objects only (0.3.7 / 0.3.DL). Looks best at night - the sparks
 * are SA-MP's glowing light objects.
 *
 * Usage: #include <fireworks> after a_samp (and after Pawn.CMD if you use it).
 * Commands: /fireworks (/fw), /fw <type> <colour> [big] [throw], /fwlist,
 *           /fwlight, /fwshow [sec], /fwstop
 *
 * Optional callbacks your gamemode can provide:
 *   public FW_OnFireworkBurst(Float:x, Float:y, Float:z)   -> every burst (e.g. alert NPCs)
 *   public FW_OnShowStart(playerid, seconds)               -> someone started /fwshow
 */

#if defined _fireworks_included
	#endinput
#endif
#define _fireworks_included

// ============================================================================
// SETTINGS
// ============================================================================
#if !defined FW_DIALOG_BASE
	#define FW_DIALOG_BASE        27300   // dialog ids used: FW_DIALOG_BASE .. +3 (change if they clash)
#endif
#if !defined FW_MAX_PLACED
	#define FW_MAX_PLACED         40      // fireworks standing on the ground at once (whole server)
#endif
#if !defined FW_MAX_PER_PLAYER
	#define FW_MAX_PER_PLAYER     5       // unlit fireworks one player can have placed
#endif
#if !defined FW_MAX_PARTICLES
	#define FW_MAX_PARTICLES      650     // cap on live spark objects (SA-MP allows 1000 objects in total)
#endif
#if !defined FW_PLAYER_COOLDOWN
	#define FW_PLAYER_COOLDOWN    1500    // ms between placing/throwing per player
#endif
#if !defined FW_FUSE_TIME
	#define FW_FUSE_TIME          4000    // ms after lighting before it launches
#endif
#if !defined FW_THROW_FUSE
	#define FW_THROW_FUSE         2500    // ms a thrown firework sits on the ground before launching
#endif
#if !defined FW_LIGHT_RANGE
	#define FW_LIGHT_RANGE        3.0     // how close you must be to /fwlight
#endif
#if !defined FW_DRAW_DISTANCE
	#define FW_DRAW_DISTANCE      400.0   // how far away the sparks are visible
#endif

// ============================================================================
// DATA
// ============================================================================
// Colour objects: glowing point lights. [colour][0] = small spark, [1] = big spark.
#define FW_COL_WHITE 0
#define FW_COL_RED   1
#define FW_COL_GREEN 2
#define FW_COL_BLUE  3
static const FW_LightModel[4][2] = {
	{19281, 19295}, // white
	{19282, 19296}, // red
	{19283, 19297}, // green
	{19284, 19298}  // blue
};

// Colour schemes offered in the menu: two colours each, FW_SCHEME_RAINBOW picks randomly per spark.
#define FW_SCHEME_RAINBOW 8
static const FW_SchemeName[][] = {
	"White", "Red", "Green", "Blue",
	"Red & White", "Blue & White", "Green & White", "Red & Blue",
	"Rainbow"
};
static const FW_SchemeColours[][2] = {
	{FW_COL_WHITE, FW_COL_WHITE}, {FW_COL_RED, FW_COL_RED}, {FW_COL_GREEN, FW_COL_GREEN}, {FW_COL_BLUE, FW_COL_BLUE},
	{FW_COL_RED, FW_COL_WHITE}, {FW_COL_BLUE, FW_COL_WHITE}, {FW_COL_GREEN, FW_COL_WHITE}, {FW_COL_RED, FW_COL_BLUE},
	{FW_COL_WHITE, FW_COL_WHITE} // rainbow (ignored)
};

// Firework types
enum
{
	FW_PEONY,         // classic sphere
	FW_CHRYSANTHEMUM, // two-colour double sphere
	FW_RING,          // tilted ring
	FW_WILLOW,        // sparks that hang and weep down slowly
	FW_PALM,          // arms curving down like a palm tree
	FW_CROSSETTE,     // bursts into 4 that each burst again
	FW_HEART,         // heart shape
	FW_STAR,          // 5-point star
	FW_GLITTER,       // crackling strobe flashes
	FW_CAKE,          // ground cake: 12 shots in a fan, mixed shells
	FW_FOUNTAIN,      // ground fountain of sparks
	FW_TYPE_COUNT
};
static const FW_TypeName[FW_TYPE_COUNT][] = {
	"Peony (sphere)", "Chrysanthemum (double sphere)", "Ring", "Willow (weeping)", "Palm",
	"Crossette (splits in 4)", "Heart", "Star", "Glitter (strobe)", "Cake (12 shots)", "Fountain"
};

// Particle bookkeeping, indexed by object id
enum
{
	FWP_NONE,
	FWP_SPARK_OUT,   // flying out from the burst
	FWP_SPARK_FALL,  // drifting down, destroyed on arrival
	FWP_ROCKET,      // rocket body climbing to its burst point
	FWP_TRAIL,       // smoke trail moving with a rocket
	FWP_CROSS_ARM    // crossette sub-shell flying out before its own burst
};
static FW_ObjKind[MAX_OBJECTS];
static Float:FW_ObjFall[MAX_OBJECTS];     // how far a spark drops after flying out
static Float:FW_ObjFallSpeed[MAX_OBJECTS];
static FW_ObjBurstType[MAX_OBJECTS];      // rockets: shell to burst into
static FW_ObjScheme[MAX_OBJECTS];
static bool:FW_ObjBig[MAX_OBJECTS];
static FW_ObjTrail[MAX_OBJECTS];          // rockets: their trail object
static FW_LiveParticles = 0;

// Placed fireworks (on the ground)
enum E_FW_PLACED
{
	bool:fpUsed,
	fpOwner,
	fpType,
	fpScheme,
	bool:fpBig,
	bool:fpLit,
	Float:fpX,
	Float:fpY,
	Float:fpZ,
	fpBase,
	fpTube,
	fpFuseFx
};
static FW_Placed[FW_MAX_PLACED][E_FW_PLACED];

// Per-player menu choices
static FW_PickType[MAX_PLAYERS];
static FW_PickScheme[MAX_PLAYERS];
static bool:FW_PickBig[MAX_PLAYERS];
static FW_LastUse[MAX_PLAYERS];
static FW_OpenDialog[MAX_PLAYERS] = {-1, ...}; // fireworks dialog this player was really shown

// Thrown fireworks in flight
#define FW_MAX_THROWN 16
enum E_FW_THROWN
{
	bool:ftUsed,
	ftObj,
	ftOwner,
	ftType,
	ftScheme,
	bool:ftBig,
	Float:ftX, Float:ftY, Float:ftZ,
	Float:ftVX, Float:ftVY, Float:ftVZ,
	Float:ftGround,
	ftTimer
};
static FW_Thrown[FW_MAX_THROWN][E_FW_THROWN];

// Firework show
static FW_ShowTimer = 0;
static FW_ShowEnds = 0;
static Float:FW_ShowX, Float:FW_ShowY, Float:FW_ShowZ;

// ============================================================================
// SMALL HELPERS
// ============================================================================
static Float:FW_RandF(Float:min, Float:max)
{
	return min + (max - min) * (float(random(10001)) / 10000.0);
}

static FW_SparkModel(scheme, bool:second, bool:big)
{
	new col;
	if(scheme == FW_SCHEME_RAINBOW) col = random(4);
	else col = FW_SchemeColours[scheme][second ? 1 : 0];
	return FW_LightModel[col][big ? 1 : 0];
}

static FW_Track(objectid, kind)
{
	if(objectid == INVALID_OBJECT_ID || objectid < 0 || objectid >= MAX_OBJECTS) return 0;
	FW_ObjKind[objectid] = kind;
	FW_LiveParticles++;
	return 1;
}

static FW_Untrack(objectid)
{
	if(objectid < 0 || objectid >= MAX_OBJECTS || FW_ObjKind[objectid] == FWP_NONE) return 0;
	FW_ObjKind[objectid] = FWP_NONE;
	FW_LiveParticles--;
	DestroyObject(objectid);
	return 1;
}

// Spawns one spark at (x,y,z) and sends it to (tx,ty,tz); afterwards it drops `fall` metres.
static FW_Spark(model, Float:x, Float:y, Float:z, Float:tx, Float:ty, Float:tz, Float:speed, Float:fall, Float:fallSpeed)
{
	if(FW_LiveParticles >= FW_MAX_PARTICLES) return 0;
	new obj = CreateObject(model, x, y, z, 0.0, 0.0, 0.0, FW_DRAW_DISTANCE);
	if(obj == INVALID_OBJECT_ID) return 0;
	FW_Track(obj, FWP_SPARK_OUT);
	FW_ObjFall[obj] = fall;
	FW_ObjFallSpeed[obj] = fallSpeed;
	MoveObject(obj, tx, ty, tz, speed);
	return 1;
}

// The bang + flash at the burst point (high up, so it hurts nobody on the ground).
static FW_Bang(Float:x, Float:y, Float:z)
{
	CreateExplosion(x, y, z, 12, 1.0);
}

static FW_PlayerCanUse(playerid)
{
	if(GetPlayerInterior(playerid) != 0) { SendClientMessage(playerid, 0xFF6347FF, "Fireworks only work outside."); return 0; }
	if(IsPlayerInAnyVehicle(playerid)) { SendClientMessage(playerid, 0xFF6347FF, "Get out of the vehicle first."); return 0; }
	if(GetTickCount() - FW_LastUse[playerid] < FW_PLAYER_COOLDOWN) { SendClientMessage(playerid, 0xFF6347FF, "Slow down!"); return 0; }
	return 1;
}

// ============================================================================
// BURST SHAPES  (centre x,y,z)
// ============================================================================
// Evenly spread points on a sphere (golden-angle spiral), flown out to `radius`.
static FW_BurstSphere(Float:x, Float:y, Float:z, Float:radius, count, scheme, bool:second, bool:big, Float:fall, Float:fallSpeed)
{
	new Float:golden = 137.50776;
	for(new i = 0; i < count; i++)
	{
		new Float:k = float(i) + 0.5;
		new Float:cz = 1.0 - 2.0 * k / float(count);          // -1..1
		new Float:r = floatsqroot(1.0 - cz * cz);
		new Float:ang = golden * float(i);
		new Float:rad = radius * FW_RandF(0.88, 1.08);         // slight irregularity looks natural
		FW_Spark(FW_SparkModel(scheme, second, big), x, y, z,
			x + rad * r * floatcos(ang, degrees), y + rad * r * floatsin(ang, degrees), z + rad * cz,
			rad * 1.25, fall, fallSpeed);
	}
}

static FW_BurstRing(Float:x, Float:y, Float:z, Float:radius, count, scheme, bool:big)
{
	new Float:tilt = FW_RandF(10.0, 60.0), Float:yaw = FW_RandF(0.0, 360.0);
	for(new i = 0; i < count; i++)
	{
		new Float:a = 360.0 * float(i) / float(count);
		// circle in the XY plane, tilted around the X axis, then turned by yaw
		new Float:px = radius * floatcos(a, degrees);
		new Float:py = radius * floatsin(a, degrees) * floatcos(tilt, degrees);
		new Float:pz = radius * floatsin(a, degrees) * floatsin(tilt, degrees);
		new Float:rx = px * floatcos(yaw, degrees) - py * floatsin(yaw, degrees);
		new Float:ry = px * floatsin(yaw, degrees) + py * floatcos(yaw, degrees);
		FW_Spark(FW_SparkModel(scheme, (i % 2) == 1, big), x, y, z, x + rx, y + ry, z + pz, radius * 1.3, 4.0, 3.0);
	}
}

// Shape drawn in a vertical plane facing `yaw`. pts are (horizontal, vertical) pairs in -1..1.
static FW_BurstPlane(Float:x, Float:y, Float:z, const Float:pts[][2], count, Float:scale, scheme, bool:big)
{
	new Float:yaw = FW_RandF(0.0, 360.0);
	for(new i = 0; i < count; i++)
	{
		new Float:h = pts[i][0] * scale, Float:v = pts[i][1] * scale;
		FW_Spark(FW_SparkModel(scheme, (i % 3) == 0, big), x, y, z,
			x + h * floatcos(yaw, degrees), y + h * floatsin(yaw, degrees), z + v, scale * 1.1, 3.0, 2.0);
	}
}

static FW_BurstHeart(Float:x, Float:y, Float:z, scheme, bool:big)
{
	new Float:pts[32][2];
	for(new i = 0; i < sizeof(pts); i++)
	{
		new Float:t = 360.0 * float(i) / float(sizeof(pts));
		new Float:s = floatsin(t, degrees);
		pts[i][0] = 16.0 * s * s * s / 17.0;
		pts[i][1] = (13.0 * floatcos(t, degrees) - 5.0 * floatcos(2.0 * t, degrees) - 2.0 * floatcos(3.0 * t, degrees) - floatcos(4.0 * t, degrees)) / 17.0;
	}
	FW_BurstPlane(x, y, z, pts, sizeof(pts), 13.0, scheme, big);
}

static FW_BurstStar(Float:x, Float:y, Float:z, scheme, bool:big)
{
	// 10 corners (outer/inner alternating), 3 points along each edge
	new Float:corner[10][2];
	for(new c = 0; c < 10; c++)
	{
		new Float:a = 90.0 + 36.0 * float(c);
		new Float:r = (c % 2 == 0) ? 1.0 : 0.42;
		corner[c][0] = r * floatcos(a, degrees);
		corner[c][1] = r * floatsin(a, degrees);
	}
	new Float:pts[30][2], n = 0;
	for(new c = 0; c < 10; c++)
	{
		new d = (c + 1) % 10;
		for(new s = 0; s < 3; s++)
		{
			new Float:f = float(s) / 3.0;
			pts[n][0] = corner[c][0] + (corner[d][0] - corner[c][0]) * f;
			pts[n][1] = corner[c][1] + (corner[d][1] - corner[c][1]) * f;
			n++;
		}
	}
	FW_BurstPlane(x, y, z, pts, n, 14.0, scheme, big);
}

static FW_BurstPalm(Float:x, Float:y, Float:z, scheme, bool:big)
{
	// 7 arms; each arm is 5 sparks travelling increasingly far and then dropping (curving look)
	for(new arm = 0; arm < 7; arm++)
	{
		new Float:yaw = 360.0 * float(arm) / 7.0 + FW_RandF(-8.0, 8.0);
		new Float:up = FW_RandF(15.0, 40.0);
		for(new s = 1; s <= 5; s++)
		{
			new Float:d = 3.2 * float(s);
			new Float:hz = d * floatcos(up, degrees), Float:vz = d * floatsin(up, degrees);
			FW_Spark(FW_SparkModel(scheme, s == 5, big), x, y, z,
				x + hz * floatcos(yaw, degrees), y + hz * floatsin(yaw, degrees), z + vz,
				d * 1.2, 4.0 + float(s) * 1.6, 3.0);
		}
	}
}

static FW_BurstGlitter(Float:x, Float:y, Float:z, scheme)
{
	// a sphere of tiny sparks plus staggered crackle flashes
	FW_BurstSphere(x, y, z, 11.0, 26, scheme, false, false, 3.0, 2.5);
	for(new i = 0; i < 10; i++)
		SetTimerEx("FW_GlitterFlash", 250 + i * 130, false, "fffd", x + FW_RandF(-9.0, 9.0), y + FW_RandF(-9.0, 9.0), z + FW_RandF(-7.0, 5.0), scheme);
}

forward FW_GlitterFlash(Float:x, Float:y, Float:z, scheme);
public FW_GlitterFlash(Float:x, Float:y, Float:z, scheme)
{
	// a short-lived flash: tiny hop, then gone
	FW_Spark(FW_SparkModel(scheme, true, true), x, y, z, x, y, z + 0.4, 2.0, 0.3, 1.0);
	if(random(2)) FW_Bang(x, y, z);
	return 1;
}

static FW_BurstCrossette(Float:x, Float:y, Float:z, scheme, bool:big)
{
	// 4 bright sub-shells fly out sideways and each pops into its own small sphere
	new Float:yaw = FW_RandF(0.0, 90.0);
	for(new i = 0; i < 4; i++)
	{
		if(FW_LiveParticles >= FW_MAX_PARTICLES) break;
		new Float:a = yaw + 90.0 * float(i);
		new obj = CreateObject(FW_SparkModel(scheme, false, true), x, y, z, 0.0, 0.0, 0.0, FW_DRAW_DISTANCE);
		if(obj == INVALID_OBJECT_ID) continue;
		FW_Track(obj, FWP_CROSS_ARM);
		FW_ObjScheme[obj] = scheme;
		FW_ObjBig[obj] = big;
		MoveObject(obj, x + 12.0 * floatcos(a, degrees), y + 12.0 * floatsin(a, degrees), z + FW_RandF(-2.0, 4.0), 16.0);
	}
}

static FW_Burst(type, Float:x, Float:y, Float:z, scheme, bool:big)
{
	FW_Bang(x, y, z);
	if(funcidx("FW_OnFireworkBurst") != -1) CallLocalFunction("FW_OnFireworkBurst", "fff", x, y, z);
	new Float:size = big ? 1.35 : 1.0;
	switch(type)
	{
		case FW_PEONY:         FW_BurstSphere(x, y, z, 13.0 * size, big ? 48 : 36, scheme, false, big, 4.0, 3.0);
		case FW_CHRYSANTHEMUM:
		{
			FW_BurstSphere(x, y, z, 8.0 * size, 22, scheme, false, big, 3.0, 2.5);
			FW_BurstSphere(x, y, z, 16.0 * size, big ? 44 : 34, scheme, true, big, 5.0, 3.0);
		}
		case FW_RING:          FW_BurstRing(x, y, z, 14.0 * size, 28, scheme, big);
		case FW_WILLOW:        FW_BurstSphere(x, y, z, 10.0 * size, 34, scheme, false, big, 18.0, 3.5); // long slow weep
		case FW_PALM:          FW_BurstPalm(x, y, z, scheme, big);
		case FW_CROSSETTE:     FW_BurstCrossette(x, y, z, scheme, big);
		case FW_HEART:         FW_BurstHeart(x, y, z, scheme, big);
		case FW_STAR:          FW_BurstStar(x, y, z, scheme, big);
		case FW_GLITTER:       FW_BurstGlitter(x, y, z, scheme);
		default:               FW_BurstSphere(x, y, z, 13.0, 32, scheme, false, big, 4.0, 3.0);
	}
}

// ============================================================================
// ROCKETS
// ============================================================================
// Launches a rocket from (x,y,z) that bursts into `burstType` after climbing `height` metres,
// leaning by up to `lean` metres sideways along `leanYaw`.
static FW_LaunchRocket(Float:x, Float:y, Float:z, burstType, scheme, bool:big, Float:height, Float:leanYaw = 0.0, Float:lean = 0.0)
{
	if(FW_LiveParticles + 2 >= FW_MAX_PARTICLES) return 0;
	new body = CreateObject(3790, x, y, z + 0.6, 0.0, 90.0, 0.0, FW_DRAW_DISTANCE);
	if(body == INVALID_OBJECT_ID) return 0;
	new trail = CreateObject(18728, x, y, z + 0.2, 0.0, 0.0, 0.0, FW_DRAW_DISTANCE);
	FW_Track(body, FWP_ROCKET);
	if(trail != INVALID_OBJECT_ID) FW_Track(trail, FWP_TRAIL);
	FW_ObjBurstType[body] = burstType;
	FW_ObjScheme[body] = scheme;
	FW_ObjBig[body] = big;
	FW_ObjTrail[body] = trail;

	new Float:tx = x + lean * floatcos(leanYaw, degrees) + FW_RandF(-2.0, 2.0);
	new Float:ty = y + lean * floatsin(leanYaw, degrees) + FW_RandF(-2.0, 2.0);
	new Float:tz = z + height;
	new Float:speed = FW_RandF(26.0, 32.0);
	MoveObject(body, tx, ty, tz, speed, 0.0, 90.0, FW_RandF(0.0, 360.0));
	if(trail != INVALID_OBJECT_ID) MoveObject(trail, tx, ty, tz - 0.6, speed);

	// launch "whoosh" for anyone close
	for(new p = 0, pmax = GetPlayerPoolSize(); p <= pmax; p++)
		if(IsPlayerConnected(p) && IsPlayerInRangeOfPoint(p, 60.0, x, y, z)) PlayerPlaySound(p, 1095, x, y, z);
	return 1;
}

forward FW_CakeShot(Float:x, Float:y, Float:z, scheme, bool:big, shot);
public FW_CakeShot(Float:x, Float:y, Float:z, scheme, bool:big, shot)
{
	// fan of 12 shots sweeping left-right, cycling through the nicest shells
	static const cakeShells[] = {FW_PEONY, FW_RING, FW_CHRYSANTHEMUM, FW_PALM, FW_PEONY, FW_STAR, FW_WILLOW, FW_RING, FW_CROSSETTE, FW_HEART, FW_GLITTER, FW_CHRYSANTHEMUM};
	new Float:fan = -45.0 + 90.0 * float(shot % 6) / 5.0;
	FW_LaunchRocket(x, y, z, cakeShells[shot % sizeof(cakeShells)], scheme, big, FW_RandF(38.0, 52.0), 90.0 + fan, 10.0);
	if(shot + 1 < 12) SetTimerEx("FW_CakeShot", 650, false, "fffddd", x, y, z, scheme, big, shot + 1);
	return 1;
}

forward FW_FountainTick(Float:x, Float:y, Float:z, scheme, ticksLeft);
public FW_FountainTick(Float:x, Float:y, Float:z, scheme, ticksLeft)
{
	for(new i = 0; i < 4; i++)
	{
		new Float:a = FW_RandF(0.0, 360.0), Float:d = FW_RandF(0.3, 2.4), Float:h = FW_RandF(4.0, 9.0);
		FW_Spark(FW_SparkModel(scheme, i % 2 == 1, false), x, y, z + 0.5,
			x + d * floatcos(a, degrees), y + d * floatsin(a, degrees), z + h, FW_RandF(9.0, 14.0), h - 0.3, FW_RandF(5.0, 8.0));
	}
	if(ticksLeft > 0) SetTimerEx("FW_FountainTick", 150, false, "fffdd", x, y, z, scheme, ticksLeft - 1);
	return 1;
}

// ============================================================================
// PLACED FIREWORKS
// ============================================================================
static FW_CountPlaced(playerid)
{
	new n = 0;
	for(new i = 0; i < FW_MAX_PLACED; i++) if(FW_Placed[i][fpUsed] && !FW_Placed[i][fpLit] && FW_Placed[i][fpOwner] == playerid) n++;
	return n;
}

static FW_Place(playerid, Float:x, Float:y, Float:z, type, scheme, bool:big)
{
	new slot = -1;
	for(new i = 0; i < FW_MAX_PLACED; i++) if(!FW_Placed[i][fpUsed]) { slot = i; break; }
	if(slot == -1) return -1;
	FW_Placed[slot][fpUsed] = true;
	FW_Placed[slot][fpOwner] = playerid;
	FW_Placed[slot][fpType] = type;
	FW_Placed[slot][fpScheme] = scheme;
	FW_Placed[slot][fpBig] = big;
	FW_Placed[slot][fpLit] = false;
	FW_Placed[slot][fpX] = x;
	FW_Placed[slot][fpY] = y;
	FW_Placed[slot][fpZ] = z;
	FW_Placed[slot][fpFuseFx] = INVALID_OBJECT_ID;
	FW_Placed[slot][fpTube] = INVALID_OBJECT_ID;
	// a box for cakes/fountains, a box with a rocket standing in it for everything else
	FW_Placed[slot][fpBase] = CreateObject((type == FW_CAKE) ? 2902 : 1271, x, y, z - 0.65, 0.0, 0.0, FW_RandF(0.0, 360.0), 150.0);
	if(type != FW_CAKE && type != FW_FOUNTAIN)
		FW_Placed[slot][fpTube] = CreateObject(3790, x, y, z + 0.95, 0.0, 90.0, 0.0, 150.0);
	return slot;
}

static FW_RemovePlaced(slot)
{
	if(!FW_Placed[slot][fpUsed]) return 0;
	if(FW_Placed[slot][fpBase] != INVALID_OBJECT_ID) DestroyObject(FW_Placed[slot][fpBase]);
	if(FW_Placed[slot][fpTube] != INVALID_OBJECT_ID) DestroyObject(FW_Placed[slot][fpTube]);
	if(FW_Placed[slot][fpFuseFx] != INVALID_OBJECT_ID) DestroyObject(FW_Placed[slot][fpFuseFx]);
	FW_Placed[slot][fpUsed] = false;
	return 1;
}

static FW_Light(slot, fuseMs)
{
	if(!FW_Placed[slot][fpUsed] || FW_Placed[slot][fpLit]) return 0;
	FW_Placed[slot][fpLit] = true;
	// burning fuse sparkle
	FW_Placed[slot][fpFuseFx] = CreateObject(18727, FW_Placed[slot][fpX], FW_Placed[slot][fpY], FW_Placed[slot][fpZ] - 0.4, 0.0, 0.0, 0.0, 150.0);
	SetTimerEx("FW_FuseDone", fuseMs, false, "d", slot);
	return 1;
}

forward FW_FuseDone(slot);
public FW_FuseDone(slot)
{
	if(!FW_Placed[slot][fpUsed]) return 0;
	new Float:x = FW_Placed[slot][fpX], Float:y = FW_Placed[slot][fpY], Float:z = FW_Placed[slot][fpZ];
	new type = FW_Placed[slot][fpType], scheme = FW_Placed[slot][fpScheme], bool:big = FW_Placed[slot][fpBig];
	FW_RemovePlaced(slot);
	switch(type)
	{
		case FW_CAKE:     FW_CakeShot(x, y, z, scheme, big, 0);
		case FW_FOUNTAIN: FW_FountainTick(x, y, z - 0.6, scheme, 100); // ~15 seconds
		default:          FW_LaunchRocket(x, y, z, type, scheme, big, FW_RandF(42.0, 58.0));
	}
	return 1;
}

// ============================================================================
// THROWING  (simple ballistic arc, stepped every 50ms)
// ============================================================================
static FW_Throw(playerid, type, scheme, bool:big)
{
	new slot = -1;
	for(new i = 0; i < FW_MAX_THROWN; i++) if(!FW_Thrown[i][ftUsed]) { slot = i; break; }
	if(slot == -1) return 0;
	new Float:x, Float:y, Float:z, Float:a;
	GetPlayerPos(playerid, x, y, z);
	GetPlayerFacingAngle(playerid, a);
	new Float:power = 11.0;
	FW_Thrown[slot][ftUsed] = true;
	FW_Thrown[slot][ftOwner] = playerid;
	FW_Thrown[slot][ftType] = type;
	FW_Thrown[slot][ftScheme] = scheme;
	FW_Thrown[slot][ftBig] = big;
	FW_Thrown[slot][ftX] = x + 0.6 * floatsin(-a, degrees);
	FW_Thrown[slot][ftY] = y + 0.6 * floatcos(-a, degrees);
	FW_Thrown[slot][ftZ] = z + 0.6;
	FW_Thrown[slot][ftVX] = power * floatsin(-a, degrees);
	FW_Thrown[slot][ftVY] = power * floatcos(-a, degrees);
	FW_Thrown[slot][ftVZ] = 6.0;
	FW_Thrown[slot][ftGround] = z - 1.0; // feet level; good enough on flat ground
	FW_Thrown[slot][ftObj] = CreateObject(1271, FW_Thrown[slot][ftX], FW_Thrown[slot][ftY], FW_Thrown[slot][ftZ], 0.0, 0.0, a, 150.0);
	ApplyAnimation(playerid, "GRENADE", "WEAPON_throw", 4.1, 0, 0, 0, 0, 0, 1);
	FW_Thrown[slot][ftTimer] = SetTimerEx("FW_ThrowStep", 50, true, "d", slot);
	return 1;
}

forward FW_ThrowStep(slot);
public FW_ThrowStep(slot)
{
	if(!FW_Thrown[slot][ftUsed]) return 0;
	new Float:dt = 0.05;
	FW_Thrown[slot][ftVZ] -= 9.8 * dt;
	FW_Thrown[slot][ftX] += FW_Thrown[slot][ftVX] * dt;
	FW_Thrown[slot][ftY] += FW_Thrown[slot][ftVY] * dt;
	FW_Thrown[slot][ftZ] += FW_Thrown[slot][ftVZ] * dt;
	new obj = FW_Thrown[slot][ftObj];
	if(FW_Thrown[slot][ftZ] <= FW_Thrown[slot][ftGround] + 0.65)
	{
		// landed: it becomes a placed firework with a short fuse
		KillTimer(FW_Thrown[slot][ftTimer]);
		if(obj != INVALID_OBJECT_ID) DestroyObject(obj);
		FW_Thrown[slot][ftUsed] = false;
		new p = FW_Place(FW_Thrown[slot][ftOwner], FW_Thrown[slot][ftX], FW_Thrown[slot][ftY], FW_Thrown[slot][ftGround] + 0.65,
			FW_Thrown[slot][ftType], FW_Thrown[slot][ftScheme], FW_Thrown[slot][ftBig]);
		if(p != -1) FW_Light(p, FW_THROW_FUSE);
		return 1;
	}
	if(obj != INVALID_OBJECT_ID)
		SetObjectPos(obj, FW_Thrown[slot][ftX], FW_Thrown[slot][ftY], FW_Thrown[slot][ftZ]);
	return 1;
}

// ============================================================================
// FIREWORK SHOW
// ============================================================================
forward FW_ShowTick();
public FW_ShowTick()
{
	new now = GetTickCount();
	new bool:finale = (FW_ShowEnds - now < 6000);
	new shots = finale ? 3 : 1 + random(2);
	for(new i = 0; i < shots; i++)
	{
		new type = random(9); // all aerial shells
		new scheme = random(sizeof(FW_SchemeName));
		FW_LaunchRocket(FW_ShowX + FW_RandF(-25.0, 25.0), FW_ShowY + FW_RandF(-25.0, 25.0), FW_ShowZ,
			type, scheme, finale || random(3) == 0, FW_RandF(40.0, 70.0), FW_RandF(0.0, 360.0), FW_RandF(0.0, 12.0));
	}
	if(now >= FW_ShowEnds)
	{
		KillTimer(FW_ShowTimer);
		FW_ShowTimer = 0;
	}
	return 1;
}

static FW_ClearAll()
{
	if(FW_ShowTimer) { KillTimer(FW_ShowTimer); FW_ShowTimer = 0; }
	for(new i = 0; i < FW_MAX_PLACED; i++) FW_RemovePlaced(i);
	for(new i = 0; i < FW_MAX_THROWN; i++)
	{
		if(!FW_Thrown[i][ftUsed]) continue;
		KillTimer(FW_Thrown[i][ftTimer]);
		if(FW_Thrown[i][ftObj] != INVALID_OBJECT_ID) DestroyObject(FW_Thrown[i][ftObj]);
		FW_Thrown[i][ftUsed] = false;
	}
	for(new o = 0; o < MAX_OBJECTS; o++) FW_Untrack(o);
	FW_LiveParticles = 0;
}

// ============================================================================
// OBJECT MOVEMENT  (drives every stage of the effects)
// ============================================================================
static FW_HandleObjectMoved(objectid)
{
	if(objectid < 0 || objectid >= MAX_OBJECTS || FW_ObjKind[objectid] == FWP_NONE) return 0;
	switch(FW_ObjKind[objectid])
	{
		case FWP_SPARK_OUT:
		{
			// flown out: now drift down (gravity) with a little sideways sway, then vanish
			new Float:x, Float:y, Float:z;
			GetObjectPos(objectid, x, y, z);
			FW_ObjKind[objectid] = FWP_SPARK_FALL;
			MoveObject(objectid, x + FW_RandF(-1.0, 1.0), y + FW_RandF(-1.0, 1.0), z - FW_ObjFall[objectid], FW_ObjFallSpeed[objectid]);
		}
		case FWP_SPARK_FALL, FWP_TRAIL: FW_Untrack(objectid);
		case FWP_ROCKET:
		{
			new Float:x, Float:y, Float:z;
			GetObjectPos(objectid, x, y, z);
			new type = FW_ObjBurstType[objectid], scheme = FW_ObjScheme[objectid], bool:big = FW_ObjBig[objectid];
			new trail = FW_ObjTrail[objectid];
			FW_Untrack(objectid);
			if(trail != INVALID_OBJECT_ID && trail >= 0 && trail < MAX_OBJECTS) FW_Untrack(trail);
			FW_Burst(type, x, y, z, scheme, big);
		}
		case FWP_CROSS_ARM:
		{
			new Float:x, Float:y, Float:z;
			GetObjectPos(objectid, x, y, z);
			new scheme = FW_ObjScheme[objectid], bool:big = FW_ObjBig[objectid];
			FW_Untrack(objectid);
			FW_Bang(x, y, z);
			FW_BurstSphere(x, y, z, 6.0, 12, scheme, true, big, 3.0, 2.5);
		}
	}
	return 1;
}

// ============================================================================
// MENU (dialogs)
// ============================================================================
#define FW_DLG_TYPE   (FW_DIALOG_BASE)
#define FW_DLG_COLOUR (FW_DIALOG_BASE + 1)
#define FW_DLG_SIZE   (FW_DIALOG_BASE + 2)
#define FW_DLG_ACTION (FW_DIALOG_BASE + 3)

// Every fireworks dialog goes through here so a response we never asked for can be ignored.
static FW_Dialog(playerid, dialogid, style, caption[], info[], button1[], button2[])
{
	FW_OpenDialog[playerid] = dialogid;
	return ShowPlayerDialog(playerid, dialogid, style, caption, info, button1, button2);
}

static FW_ShowTypeMenu(playerid)
{
	new list[512];
	for(new i = 0; i < FW_TYPE_COUNT; i++)
	{
		strcat(list, FW_TypeName[i]);
		strcat(list, "\n");
	}
	FW_Dialog(playerid, FW_DLG_TYPE, DIALOG_STYLE_LIST, "{FFD700}Fireworks {FFFFFF}- pick a type", list, "Next", "Close");
}


// Place (or throw) a firework. Shared by the menu and the /fw command.
static FW_DoAction(playerid, type, scheme, bool:big, bool:throw)
{
	if(!FW_PlayerCanUse(playerid)) return 0;
	if(!throw)
	{
		if(FW_CountPlaced(playerid) >= FW_MAX_PER_PLAYER) return SendClientMessage(playerid, 0xFF6347FF, "You already have too many fireworks set up. Light them first.");
		new Float:x, Float:y, Float:z, Float:a;
		GetPlayerPos(playerid, x, y, z);
		GetPlayerFacingAngle(playerid, a);
		x += 1.5 * floatsin(-a, degrees);
		y += 1.5 * floatcos(-a, degrees);
		if(FW_Place(playerid, x, y, z, type, scheme, big) == -1) return SendClientMessage(playerid, 0xFF6347FF, "Too many fireworks on the server right now.");
		ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 0, 0, 0, 0, 0, 1);
		new msg[144];
		format(msg, sizeof(msg), "{FFD700}[Fireworks] {FFFFFF}%s (%s) placed. Use {FFD700}/fwlight{FFFFFF} and step back!", FW_TypeName[type], FW_SchemeName[scheme]);
		SendClientMessage(playerid, -1, msg);
	}
	else
	{
		if(!FW_Throw(playerid, type, scheme, big)) return SendClientMessage(playerid, 0xFF6347FF, "Too many fireworks in the air right now.");
		SendClientMessage(playerid, -1, "{FFD700}[Fireworks] {FFFFFF}Thrown - it launches when it lands!");
	}
	FW_LastUse[playerid] = GetTickCount();
	return 1;
}

static FW_HandleDialog(playerid, dialogid, response, listitem, inputtext[])
{
	#pragma unused inputtext
	if(dialogid >= FW_DIALOG_BASE && dialogid <= FW_DIALOG_BASE + 3)
	{
		if(dialogid != FW_OpenDialog[playerid]) return 1; // not a menu we showed this player
		FW_OpenDialog[playerid] = -1;
	}
	switch(dialogid)
	{
		case FW_DLG_TYPE:
		{
			if(!response) return 1;
			if(listitem < 0 || listitem >= FW_TYPE_COUNT) return 1;
			FW_PickType[playerid] = listitem;
			new list[256];
			for(new i = 0; i < sizeof(FW_SchemeName); i++) { strcat(list, FW_SchemeName[i]); strcat(list, "\n"); }
			FW_Dialog(playerid, FW_DLG_COLOUR, DIALOG_STYLE_LIST, "{FFD700}Fireworks {FFFFFF}- pick colours", list, "Next", "Back");
			return 1;
		}
		case FW_DLG_COLOUR:
		{
			if(!response) { FW_ShowTypeMenu(playerid); return 1; }
			if(listitem < 0 || listitem >= sizeof(FW_SchemeName)) return 1;
			FW_PickScheme[playerid] = listitem;
			FW_Dialog(playerid, FW_DLG_SIZE, DIALOG_STYLE_LIST, "{FFD700}Fireworks {FFFFFF}- shell size", "Normal\nBig (brighter, wider)", "Next", "Back");
			return 1;
		}
		case FW_DLG_SIZE:
		{
			if(!response) { FW_ShowTypeMenu(playerid); return 1; }
			FW_PickBig[playerid] = (listitem == 1);
			FW_Dialog(playerid, FW_DLG_ACTION, DIALOG_STYLE_LIST, "{FFD700}Fireworks {FFFFFF}- what now?",
				"Place it here (light with /fwlight)\nThrow it (launches when it lands)", "Go", "Back");
			return 1;
		}
		case FW_DLG_ACTION:
		{
			if(!response) { FW_ShowTypeMenu(playerid); return 1; }
			FW_DoAction(playerid, FW_PickType[playerid], FW_PickScheme[playerid], FW_PickBig[playerid], listitem == 1);
			return 1;
		}
	}
	return 0;
}

// ============================================================================
// COMMANDS
// ============================================================================
// Copies the next space-separated word of `text` starting at `idx` into `word`. Returns 0 at the end.
static FW_NextWord(const text[], &idx, word[], size = sizeof(word))
{
	while(text[idx] == ' ') idx++;
	if(text[idx] == EOS) return 0;
	new n = 0;
	while(text[idx] > ' ' && n < size - 1) word[n++] = text[idx++];
	word[n] = EOS;
	while(text[idx] > ' ') idx++;
	return 1;
}

static FW_HandleCommand(playerid, const cmdtext[])
{
	new cmd[16], idx = 0;
	while(cmdtext[idx] > ' ' && idx < sizeof(cmd) - 1) { cmd[idx] = cmdtext[idx]; idx++; }
	cmd[idx] = EOS;

	if(!strcmp(cmd, "/fireworks", true) || !strcmp(cmd, "/fw", true))
	{
		// /fw <type> <colour> [big] [throw]  - works even if a gamemode blocks filterscript dialogs
		new type = 0, colour = 0, bool:big = false, bool:throw = false, word[16], n = 0;
		while(FW_NextWord(cmdtext, idx, word))
		{
			if(!strcmp(word, "big", true)) big = true;
			else if(!strcmp(word, "throw", true)) throw = true;
			else if(n == 0) { type = strval(word); n++; }
			else if(n == 1) { colour = strval(word); n++; }
		}
		if(n == 0) { FW_ShowTypeMenu(playerid); return 1; }
		if(type < 1 || type > FW_TYPE_COUNT || (n >= 2 && (colour < 1 || colour > sizeof(FW_SchemeName))))
			return SendClientMessage(playerid, 0xFF6347FF, "Usage: /fw <type 1-11> <colour 1-9> [big] [throw]  -  /fwlist shows the numbers");
		if(n < 2) colour = sizeof(FW_SchemeName); // no colour given = rainbow
		FW_DoAction(playerid, type - 1, colour - 1, big, throw);
		return 1;
	}
	if(!strcmp(cmd, "/fwlist", true))
	{
		new line[144], part[48];
		SendClientMessage(playerid, 0xFFD700FF, "Firework types:");
		line[0] = EOS;
		for(new i = 0; i < FW_TYPE_COUNT; i++)
		{
			format(part, sizeof(part), "%d=%s  ", i + 1, FW_TypeName[i]);
			if(strlen(line) + strlen(part) > 120) { SendClientMessage(playerid, -1, line); line[0] = EOS; }
			strcat(line, part);
		}
		if(line[0]) SendClientMessage(playerid, -1, line);
		SendClientMessage(playerid, 0xFFD700FF, "Colours:");
		line[0] = EOS;
		for(new i = 0; i < sizeof(FW_SchemeName); i++)
		{
			format(part, sizeof(part), "%d=%s  ", i + 1, FW_SchemeName[i]);
			strcat(line, part);
		}
		SendClientMessage(playerid, -1, line);
		SendClientMessage(playerid, -1, "Example: /fw 7 9 big throw  (heart, rainbow, big, thrown)");
		return 1;
	}
	if(!strcmp(cmd, "/fwlight", true))
	{
		new best = -1, Float:bestD = FW_LIGHT_RANGE;
		for(new i = 0; i < FW_MAX_PLACED; i++)
		{
			if(!FW_Placed[i][fpUsed] || FW_Placed[i][fpLit]) continue;
			if(FW_Placed[i][fpOwner] != playerid) continue; // only the person who placed it
			new Float:d = GetPlayerDistanceFromPoint(playerid, FW_Placed[i][fpX], FW_Placed[i][fpY], FW_Placed[i][fpZ]);
			if(d <= bestD) { bestD = d; best = i; }
		}
		if(best == -1) return SendClientMessage(playerid, 0xFF6347FF, "None of your fireworks are close enough to light.");
		FW_Light(best, FW_FUSE_TIME);
		ApplyAnimation(playerid, "BOMBER", "BOM_Plant", 4.1, 0, 0, 0, 0, 0, 1);
		SendClientMessage(playerid, -1, "{FFD700}[Fireworks] {FFFFFF}Fuse lit - get back!");
		return 1;
	}
	if(!strcmp(cmd, "/fwshow", true))
	{
		new secs = strval(cmdtext[idx]);
		if(secs < 10 || secs > 600) secs = 60;
		GetPlayerPos(playerid, FW_ShowX, FW_ShowY, FW_ShowZ);
		FW_ShowZ -= 1.0;
		FW_ShowEnds = GetTickCount() + secs * 1000;
		if(FW_ShowTimer) KillTimer(FW_ShowTimer);
		FW_ShowTimer = SetTimer("FW_ShowTick", 900, true);
		if(funcidx("FW_OnShowStart") != -1) CallLocalFunction("FW_OnShowStart", "dd", playerid, secs);
		new msg[96];
		format(msg, sizeof(msg), "{FFD700}[Fireworks] {FFFFFF}A %d second firework show is starting! Look up!", secs);
		SendClientMessageToAll(-1, msg);
		return 1;
	}
	if(!strcmp(cmd, "/fwstop", true))
	{
		FW_ClearAll();
		SendClientMessage(playerid, -1, "{FFD700}[Fireworks] {FFFFFF}All fireworks cleared.");
		return 1;
	}
	return 0;
}

// ============================================================================
// LIFECYCLE + CALLBACK HOOKS (ALS: the gamemode's own callbacks keep working)
// ============================================================================
static bool:FW_Initialised = false;

static FW_Init()
{
	if(FW_Initialised) return 1;
	FW_Initialised = true;
	for(new o = 0; o < MAX_OBJECTS; o++) FW_ObjKind[o] = FWP_NONE;
	for(new i = 0; i < FW_MAX_PLACED; i++) FW_Placed[i][fpUsed] = false;
	for(new i = 0; i < FW_MAX_THROWN; i++) FW_Thrown[i][ftUsed] = false;
	for(new p = 0; p < MAX_PLAYERS; p++) FW_OpenDialog[p] = -1;
	FW_LiveParticles = 0;
	return 1;
}

public OnGameModeInit()
{
	FW_Init();
	#if defined fw_OnGameModeInit
		return fw_OnGameModeInit();
	#else
		return 1;
	#endif
}
#if defined _ALS_OnGameModeInit
	#undef OnGameModeInit
#else
	#define _ALS_OnGameModeInit
#endif
#define OnGameModeInit fw_OnGameModeInit
#if defined fw_OnGameModeInit
	forward fw_OnGameModeInit();
#endif

public OnGameModeExit()
{
	FW_ClearAll();
	#if defined fw_OnGameModeExit
		return fw_OnGameModeExit();
	#else
		return 1;
	#endif
}
#if defined _ALS_OnGameModeExit
	#undef OnGameModeExit
#else
	#define _ALS_OnGameModeExit
#endif
#define OnGameModeExit fw_OnGameModeExit
#if defined fw_OnGameModeExit
	forward fw_OnGameModeExit();
#endif

public OnObjectMoved(objectid)
{
	if(FW_HandleObjectMoved(objectid)) return 1;
	#if defined fw_OnObjectMoved
		return fw_OnObjectMoved(objectid);
	#else
		return 1;
	#endif
}
#if defined _ALS_OnObjectMoved
	#undef OnObjectMoved
#else
	#define _ALS_OnObjectMoved
#endif
#define OnObjectMoved fw_OnObjectMoved
#if defined fw_OnObjectMoved
	forward fw_OnObjectMoved(objectid);
#endif

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
	if(dialogid >= FW_DIALOG_BASE && dialogid <= FW_DIALOG_BASE + 3)
	{
		FW_HandleDialog(playerid, dialogid, response, listitem, inputtext);
		return 1; // ours - never passed on
	}
	#if defined fw_OnDialogResponse
		return fw_OnDialogResponse(playerid, dialogid, response, listitem, inputtext);
	#else
		return 0;
	#endif
}
#if defined _ALS_OnDialogResponse
	#undef OnDialogResponse
#else
	#define _ALS_OnDialogResponse
#endif
#define OnDialogResponse fw_OnDialogResponse
#if defined fw_OnDialogResponse
	forward fw_OnDialogResponse(playerid, dialogid, response, listitem, inputtext[]);
#endif

public OnPlayerDisconnect(playerid, reason)
{
	// unlit fireworks of a leaving player are cleaned up; lit ones still go off
	for(new i = 0; i < FW_MAX_PLACED; i++)
		if(FW_Placed[i][fpUsed] && !FW_Placed[i][fpLit] && FW_Placed[i][fpOwner] == playerid) FW_RemovePlaced(i);
	FW_LastUse[playerid] = 0;
	FW_OpenDialog[playerid] = -1;
	#if defined fw_OnPlayerDisconnect
		return fw_OnPlayerDisconnect(playerid, reason);
	#else
		return 1;
	#endif
}
#if defined _ALS_OnPlayerDisconnect
	#undef OnPlayerDisconnect
#else
	#define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect fw_OnPlayerDisconnect
#if defined fw_OnPlayerDisconnect
	forward fw_OnPlayerDisconnect(playerid, reason);
#endif

// ---- commands: real Pawn.CMD commands when Pawn.CMD is used, otherwise OnPlayerCommandText
#if defined PAWNCMD_INC_
static FW_RunCommand(playerid, const name[], const params[])
{
	new text[144];
	format(text, sizeof(text), "/%s %s", name, params);
	return FW_HandleCommand(playerid, text);
}

	CMD:fireworks(playerid, params[]) return FW_RunCommand(playerid, "fireworks", params);
	CMD:fw(playerid, params[])        return FW_RunCommand(playerid, "fw", params);
	CMD:fwlist(playerid, params[])    return FW_RunCommand(playerid, "fwlist", params);
	CMD:fwlight(playerid, params[])   return FW_RunCommand(playerid, "fwlight", params);
	CMD:fwshow(playerid, params[])    return FW_RunCommand(playerid, "fwshow", params);
	CMD:fwstop(playerid, params[])    return FW_RunCommand(playerid, "fwstop", params);
#else
	public OnPlayerCommandText(playerid, cmdtext[])
	{
		if(FW_HandleCommand(playerid, cmdtext)) return 1;
		#if defined fw_OnPlayerCommandText
			return fw_OnPlayerCommandText(playerid, cmdtext);
		#else
			return 0;
		#endif
	}
	#if defined _ALS_OnPlayerCommandText
		#undef OnPlayerCommandText
	#else
		#define _ALS_OnPlayerCommandText
	#endif
	#define OnPlayerCommandText fw_OnPlayerCommandText
	#if defined fw_OnPlayerCommandText
		forward fw_OnPlayerCommandText(playerid, cmdtext[]);
	#endif
#endif
