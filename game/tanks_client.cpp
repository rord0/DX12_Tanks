#include "tanks_client.hpp"
#include "serialize.hpp"
#include "tanks.hpp"
#include "tanks_math.hpp"
#include "tanks_server.hpp"
#include "transforms.hpp"
#include "util.hpp"
#include "render_commands.hpp"
#include "prefabs.hpp"
#include "ui.hpp"
#include "ui.cpp"
#include "ui_styles.cpp"
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

mat4 orthographicProjection(float right, float left, float top, float bottom, float n, float f)
{
    mat4 m = {};
    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = 1.0f / (f - n);           // DX12 [0,1] depth
    m.m[3][0] = -((right + left) / (right - left));  // row-major translation
    m.m[3][1] = -((top + bottom) / (top - bottom));
    m.m[3][2] = -n / (f - n);
    m.m[3][3] = 1.0f;
    return m;
}                      

mat4 inverseOrthographicProjection(float right, float left, float top, float bottom, float n, float f)
{
    mat4 m = {};

    m.m[0][0] = (right - left) * 0.5f;
    m.m[1][1] = (top - bottom) * 0.5f;
    m.m[2][2] = (f - n) * 0.5f;

    m.m[0][3] = (right + left) * 0.5f;
    m.m[1][3] = (top + bottom) * 0.5f;
    m.m[2][3] = (f + n) * 0.5f;

    m.m[3][3] = 1.0f;

    return m;
}

mat4 translationMatrix(float x, float y, float z)
{
    mat4 m = {};

    m.m[0][0] = 1.0f;
    m.m[1][1] = 1.0f;
    m.m[2][2] = 1.0f;
    m.m[3][3] = 1.0f;

    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;

    return m;
}

mat4 transpose(mat4 m)
{
    mat4 result;
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
            result.m[row][col] = m.m[col][row];
    return result;
}

vec4 CalculateUVTransform(AtlasEntry entry, u32 atlasHeight, u32 atlasWidth)
{
	vec4 uv = {(f32)entry.width / (f32)atlasWidth, (f32)entry.height / (f32)atlasHeight, (f32)entry.x / (f32)atlasWidth, (f32)entry.y / (f32)atlasHeight};
	return uv;
}

f32 easeOutExpo(f32 x)  { return (x == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * x); }
f32 easeInQuad(f32 x)   { return (x * x); }
f32 easeInExpo(float x) { return x == 0.0f ? 0.0f : powf(2.0f, 10.0f * x - 10.0f); }

float Vec2AngleToRad(vec2 A, vec2 B)
{
	vec2 d = A - B;
	return atan2f(d.y, d.x);
}

float TurretRecoilOffset(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float peakTime = 0.3f;
    const float maxOffset = 0.033f;

    float y;

    if (t < peakTime)
    {
        // Going up: 0 → 0.3
        float u = t / peakTime;          // normalize to 0 → 1
        y = easeOutExpo(u) * maxOffset;
    }
    else
    {
        // Going down: 0.3 → 1.0
        float d = (t - peakTime) / (1.0f - peakTime); // normalize to 0 → 1
        y = (1.0f - easeInQuad(d)) * maxOffset;
    }

    return y;
}

void DrawHealthbar(RendererPushBuffer * cmdBuffer, vec2 center, f32 remaining, f32 flashAmt)
{
	vec3 healthBarBGColor = {0.129411765, 0.141176471, 0.145098039};
	vec3 hbRemainingColor = ColorHexToRBGNormalized(0x98C24D);
	vec3 hbLostColor = ColorHexToRBGNormalized(0x8D2E24);
	f32 healthBarWidth = 0.8f;
	f32 healthBarHeight = 0.06f;
	f32 lost = 1.0f - remaining;
	f32 hbLeftside = center.x - (healthBarWidth/4.0f);
	f32 hbRightside = center.x + (healthBarWidth/4.0f);

    DebugGeoInstanceData hbBackground= {vec3{center.x, center.y, 0.0f}, {healthBarWidth + 0.026f, healthBarHeight + 0.026f}, healthBarBGColor, 0, 1.0f};
    DebugGeoInstanceData hbRemaining = {vec3{hbLeftside + ((healthBarWidth*remaining)/4.0f),center.y, 0.0f}, {healthBarWidth*remaining, healthBarHeight}, hbRemainingColor, 0, 1.0f};
    DebugGeoInstanceData hbFlash = {vec3{hbLeftside + ((healthBarWidth*flashAmt)/4.0f),center.y, 0.0f}, {healthBarWidth*flashAmt, healthBarHeight}, vec3{1.0f,1.0f,1.0f}, 0, 1.0f};
    DebugGeoInstanceData hbLost = {vec3{hbRightside - ((healthBarWidth*lost)/4.0f), center.y, 0.0f}, {healthBarWidth*lost, healthBarHeight}, hbLostColor, 0, 1.0f};

	RendererPushRectangle(cmdBuffer, hbBackground, 3);
	RendererPushRectangle(cmdBuffer, hbLost, 4);
	RendererPushRectangle(cmdBuffer, hbFlash, 4);
	RendererPushRectangle(cmdBuffer, hbRemaining, 5);
}

void CalculateTankUVs(TankStyle style, AtlasEntry * atlasEntries, vec4 * uvs)
{
	const u32 atlasWidth  = 4096;
	const u32 atlasHeight = 4096;

    u32 trackIndex  =  0 + (4 * style.trackType)  + style.colorID;
    u32 bodyIndex   = 12 + (4 * style.bodyType)   + style.colorID;
    u32 turretIndex = 24 + (4 * style.turretType) + style.colorID;

	vec4 trackUV  = CalculateUVTransform(atlasEntries[trackIndex], atlasHeight, atlasWidth);
	vec4 bodyUV   = CalculateUVTransform(atlasEntries[bodyIndex], atlasHeight, atlasWidth);
	vec4 turretUV = CalculateUVTransform(atlasEntries[turretIndex], atlasHeight, atlasWidth);

	uvs[0] = trackUV;
	uvs[1] = bodyUV;
	uvs[2] = turretUV;
}

void DrawTank(TankGFX & tank, RendererPushBuffer * cmdBuffer, GameState * state)
{
	if (!tank.active) { return; }

    u32 trackIndex  =  0 + (3 * tank.style.trackType) + tank.style.colorID;
    u32 bodyIndex   = 12 + (3 * tank.style.bodyType ) + tank.style.colorID;
    u32 turretIndex = 24 + (3 * tank.style.trackType) + tank.style.colorID;

	//TODO(rordon): assert that indexes are less than 36.
	u32 atlasWidth  = 4096;
	u32 atlasHeight = 4096;

	vec4 uvs[3];
	CalculateTankUVs(tank.style, state->tankAtlasEntries, uvs);

	vec2 turretDir = vec2{cosf(tank.turretRot), sinf(tank.turretRot)};
	vec2 tankDir   = vec2{cosf(tank.rotation),  sinf(tank.rotation)};
	vec2 turretCenter = (-tankDir * -0.03f) + tank.position;
	vec2 turretPos = (-turretDir * 0.075f); // Offset backwards
	turretPos += (turretDir *  TurretRecoilOffset(tank.turretOffset)); // Add recoil offset.
	turretPos += turretCenter;

	f32 tankRot = tank.rotation + (PI/2.0f);

	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.8f, 1.0f}, uvs[0], 1);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.64f, 1.0f}, uvs[1], 2);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {turretPos.x, turretPos.y, 0.0f}, tank.turretRot + (PI/2.0f), {0.57f, 1.0f}, uvs[2], 3);

	DrawHealthbar(cmdBuffer, vec2{tank.position.x, tank.position.y + 0.3f}, ((float)tank.health / (float)TANK_MAX_HEALTH), tank.healthLerp);
	bool isEnemyTank = tank.playerID != state->playerID;
	vec4 nameColor = isEnemyTank ? ColorHexToRBGANormalized(0xFF6D5DFF) : ColorHexToRBGANormalized(0xFFFFFFFF);
	TextStyle textStyle = {.fillColor = nameColor, .strokeWidth = 0.27f};
	RendererPushText(cmdBuffer, tank.displayName, 0.05f, state->interFontHandle, vec2{tank.position.x - 0.2f, tank.position.y + 0.34f}, textStyle, true, 30);

	// DEBUG VISUALS
    //RendererPushCircle(cmdBuffer, vec3{turretPos.x, turretPos.y, 0}, 0, {0.05f,0.05f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);
    //RendererPushCircle(cmdBuffer, vec3{turretCenter.x, turretCenter.y, 0}, 0, {0.05f,0.05f}, {1.0f, 1.0f, 0.0f}, 1.0f, 30);
}

void DrawSpriteFrame(RendererPushBuffer * cmdBuffer, vec2 position, vec2 scale, f32 rotation, SpriteSheet * sheet, u32 spriteIndex)
{
	spriteIndex = spriteIndex % sheet->numFrames;

	const u32 animFrameWidth  = sheet->sheetWidth  / sheet->numCols;
	const u32 animFrameHeight = sheet->sheetHeight / sheet->numRows;

	u32 colIndex = spriteIndex % sheet->numCols;
	u32 rowIndex = spriteIndex / sheet->numCols;

	u32 sheetPosX = colIndex * animFrameWidth;
	u32 sheetPosY = rowIndex * animFrameHeight;

	vec4 uv = {(f32)animFrameWidth / (f32)sheet->sheetWidth, (f32)animFrameHeight / (f32)sheet->sheetHeight, (f32)sheetPosX / (f32)sheet->sheetWidth, (f32)sheetPosY / (f32)sheet->sheetHeight};

	RendererPushSubTexture(cmdBuffer, sheet->textureHandle, {position.x, position.y, 0.0f}, rotation, scale, uv, 3);
}

vec4 HSVtoRGBA(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120){ r = x; g = c; b = 0; }
    else if (h < 180){ r = 0; g = c; b = x; }
    else if (h < 240){ r = 0; g = x; b = c; }
    else if (h < 300){ r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }

    return { r + m, g + m, b + m, 1.0f};
}

vec4 GetHSVSpectrumColor(float time, float speed = 1.0f)
{
    float hue = fmod(time * speed * 60.0f, 360.0f);
    return HSVtoRGBA(hue, 1.0f, 1.0f);
}

vec2 MousePosToWorld(vec2i mousePos, vec2 cameraPos, vec2i viewportSize, mat4 proj)
{
	float aspect = (float)viewportSize.x / (float)viewportSize.y;
	vec2 mouseNDC;
	mouseNDC.x = (2.0f * (mousePos.x / (float)viewportSize.x)) - 1.0f;
	mouseNDC.y = 1.0f - ((2.0f * mousePos.y) / (float)viewportSize.y);

	vec4 mouseClip = {mouseNDC.x, mouseNDC.y, 0.0f, 1.0f};
	vec4 worldPos = inverseOrthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f) * mouseClip;

	return {worldPos.x + cameraPos.x, worldPos.y + cameraPos.y};
}

bool CSVParseU32Field(const char *& ptr, const char *& end, u32 & out)
{
    const char * start = ptr;
    while (ptr < end && *ptr != ',' && *ptr != '\n') { ptr++; }

    int value = 0;
    auto result = std::from_chars(start, ptr, value);
    if (result.ec != std::errc()) { return false; }

    // Skip comma
    if (*ptr == ',') { ptr++; }

    out = value;
    return true;
}

void ParseTextureAtlasCSV(GameMemory * memory, GameState * state, const char * filepath)
{
    // read csv file into buffer.
    DEBUG_FileResult fileResult = memory->platform.platformLoadFile(filepath);
    if (fileResult.size == 0) { return; }

    // Count the number of lines 
    u32 numLines = 0;

    for (int i = 0; i < fileResult.size; i++)
    {
        if (((char*)fileResult.data)[i] == '\n') { numLines++; };
    }
    if (((char*)fileResult.data)[fileResult.size] != '\n') { numLines++; };

	state->tankAtlasEntries = (AtlasEntry*)ArenaPush(&state->permArena, sizeof(AtlasEntry) * (numLines - 1));

    // Parse records
    const char* ptr = (char*)fileResult.data;
    const char* end = (char*)fileResult.data + fileResult.size;

    // Skip first line
    while (ptr < end && *ptr != '\n') { ptr++; }
    ptr++;

	int entryCount = 0;
    // Add x, y, frame_width, frame_height fields to AtlasEntry struct
    while (ptr < end)
    {
        // Skip name field
        while (ptr < end && *ptr != ',') { ptr++; }
        ptr++;

        // Read fields
        AtlasEntry entry = {0};
        if (!CSVParseU32Field(ptr, end, entry.x)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.y)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.width)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.height)) { break; }

        // Add entry to array
		state->tankAtlasEntries[entryCount++] = entry;
    }

    memory->platform.platformFreeFile(&fileResult.data);
}

void EmitParticles(ParticleEmitter * emitter, vec2 position, vec2 direction)
{
	for (int i = 0; i < emitter->maxParticles; i++)
	{
		Particle * particle = &emitter->particles[i];
		if (!particle->active)
		{
			particle->active = true;
			particle->timeAlive = 0.0f;
			particle->position = position;
			particle->direction = direction;
			break;
		}
	}
}

void SimulateParticles(ParticleEmitter * emitter, double deltaTime)
{
	for (int i = 0; i < emitter->maxParticles; i++)
	{
		Particle * effect = &emitter->particles[i];
		if (effect->active)
		{
			effect->timeAlive += (float)deltaTime;

			if (effect->timeAlive > emitter->startLifetime)
			{
				effect->timeAlive = emitter->startLifetime;
				effect->active = false;
			}
		}
	}
}

int GetTankIndex(u16 playerID, GameState * state)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].playerID == playerID) { return i; }
	}
	return -1;
}

void TankPlayFireEffects(u16 playerID, vec2 hitPosition, GameState * state)
{
	int tankIndex = GetTankIndex(playerID, state);
	if (tankIndex == -1) { return; }

	TankGFX * tank = &state->tanks[tankIndex];
	tank->turretOffset = 0.0f;
	vec2 particleDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
	vec2 tankDir = vec2{cosf(tank->rotation),  sinf(tank->rotation)};
	vec2 turretCenter = (-tankDir * -0.03f) + tank->position;
	vec2 newPos = turretCenter - particleDir * 0.5f;
	vec2 turretTipPos = turretCenter - particleDir * 0.25f;
	EmitParticles(&state->turretFireEmitter, newPos, particleDir);
	EmitParticles(&state->explosionEmitter, hitPosition, RandomDirection(&state->random));
	EmitParticles(&state->shellTrailEmitter, turretTipPos, hitPosition);
	EmitParticles(&state->impactEmitter, hitPosition, RandomDirection(&state->random));
}

void UpdateTankGFX(TankGFX * tank, double deltaTime)
{
	if (!(tank->active)) { return; }

	if (tank->turretOffset < 1.0f)
	{
		tank->turretOffset += deltaTime;
		if (tank->turretOffset > 1.0f) tank->turretOffset = 1.0f;
	}

	f32 healthNormalized = ((f32)tank->health / (f32)TANK_MAX_HEALTH);
	if (tank->healthLerp > healthNormalized)
	{
		tank->healthLerp -= deltaTime * 0.5f;
		if (tank->healthLerp < 0.0f) { tank->healthLerp = 0.0f; }
	}
	else if (tank->healthLerp < healthNormalized)
	{
		tank->healthLerp = healthNormalized;
	}
}


void DrawTurretFireEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->turretFireEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->turretFireEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 animProgress = effect->timeAlive / state->turretFireEmitter.startLifetime;
		u32 currentFrame = (u32)std::max(0.0f, animProgress * state->fireEffectSheet.numFrames);
		f32 aspect = 480.0f / 240.0f;
		vec2 scale = {aspect, 1.0f};
		scale *= 0.6f;
		f32 angle = atan2f(effect->direction.y, effect->direction.x);
		if (angle < 0) angle += 2 * PI;
		DrawSpriteFrame(renderCmds, effect->position, scale, angle - PI, &state->fireEffectSheet, currentFrame);
	}
}

void DrawExplosionEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->explosionEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->explosionEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 animProgress = effect->timeAlive / state->explosionEmitter.startLifetime;
		u32 currentFrame = (u32)std::max(0.0f, animProgress * state->explosionVFXSheet.numFrames);
		vec2 scale = {1.0f, 1.0f};
		f32 angle = atan2f(effect->direction.y, effect->direction.x);
		if (angle < 0) angle += 2 * PI;
		DrawSpriteFrame(renderCmds, effect->position, scale, angle, &state->explosionVFXSheet, currentFrame);
	}
}

void DrawShellTrailEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->shellTrailEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->shellTrailEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 progress = effect->timeAlive / state->shellTrailEmitter.startLifetime;
		f32 alpha = 1.0f - easeInExpo(progress);
		vec4 lineColor = {0.2078f, 0.2078f, 0.2078f, alpha / 10.0f};

		RendererPushLine(renderCmds, effect->position, effect->direction, lineColor, 0.025f, 2);
	}
}

void DrawShellImpactEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->shellTrailEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->impactEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 progress = effect->timeAlive / state->impactEmitter.startLifetime;
		f32 alpha = 1.0f - easeInExpo(progress);
		f32 angle = atan2f(effect->direction.y, effect->direction.x);

		mat4 model = ModelMatrix2D(effect->position, angle, {0.5f, 0.5f});
		RendererPushImage(renderCmds, state->shellImpactTextureHandle, alpha, model, 1);
	}
}

void DrawParticles(RendererPushBuffer * renderCmds, GameState * state)
{
	DrawTurretFireEffects(renderCmds, state);
	DrawExplosionEffects(renderCmds, state);
	DrawShellTrailEffects(renderCmds, state);
	DrawShellImpactEffects(renderCmds, state);
}

ParticleEmitter InitEmitter(f32 startLifetime, u32 maxParticles, Arena * arena)
{
	ParticleEmitter out;
	out.startLifetime = startLifetime;
	out.maxParticles = 16;
	out.particles = (Particle*)ArenaPush(arena, sizeof(Particle) * maxParticles);
	return out;
}

void ClientLoadTextures(HashMap * handles, PlatformAPI * platform)
{
	u32 textureHandle = platform->platformLoadTexture(RESOURCES_PATH"images/props/props_airdrop.png");
	HashMapInsert(handles, TEXTURE_AIRDROP_PATH, &textureHandle);

	textureHandle = platform->platformLoadTexture(RESOURCES_PATH"images/props/props_barrel.png");
	HashMapInsert(handles, TEXTURE_BARREL_PATH, &textureHandle);

	textureHandle = platform->platformLoadTexture(RESOURCES_PATH"images/props/props_building_01.png");
	HashMapInsert(handles, TEXTURE_BUILDING_LARGE_PATH, &textureHandle);
}

void ClientStart(GameState * state, GameMemory * gameMemory)
{
    state->tankAtlasHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"tank_parts.png");
    state->shellImpactTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"shell_impact.png");
    state->customizeIconTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"customize_icon.png");
    state->targetIconTextureHandle    = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"target_icon.png");
    state->hamburgerIconTextureHandle    = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"hamburger_icon.png");
    state->desertBackgroundTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"background_desert.png");

	state->textureHandles = HashMapInit(sizeof(u32), 33, ArenaPush(&state->permArena, HashMapSizeRequired(sizeof(u32), 33)));
	ClientLoadTextures(&state->textureHandles, &gameMemory->platform);

	state->interFontHandle = gameMemory->platform.loadFont(RESOURCES_PATH"fonts/Inter/Inter_18pt-Bold.png", RESOURCES_PATH"fonts/Inter/Inter_18pt-Bold.json");
    ParseTextureAtlasCSV(gameMemory, state, RESOURCES_PATH"tank_parts.csv");

	state->connected = false;
	state->helloSent = false;

	// SPRITESHEETS INITIALIZATION
	state->fireEffectSheet.textureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"tank_fire_spritesheet.png");
	state->fireEffectSheet.sheetHeight = 480;
	state->fireEffectSheet.sheetWidth  = 1080;
	state->fireEffectSheet.numFrames = 6;
	state->fireEffectSheet.numCols	 = 3;
	state->fireEffectSheet.numRows   = 2;

	state->explosionVFXSheet.textureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"explosion_spritesheet.png");
	state->explosionVFXSheet.sheetHeight = 1080;
	state->explosionVFXSheet.sheetWidth  = 2048;
	state->explosionVFXSheet.numFrames = 8;
	state->explosionVFXSheet.numCols   = 4;
	state->explosionVFXSheet.numRows   = 2;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		state->tanks[i] = {0};
	}

	const f32 spritesheetAnimFPS = 20;
	const f32 fireAnimDuration = (1.0 / spritesheetAnimFPS) * state->fireEffectSheet.numFrames;
	const f32 explosionAnimDuration = (1.0 / spritesheetAnimFPS) * state->explosionVFXSheet.numFrames;

	state->turretFireEmitter = InitEmitter(fireAnimDuration, 16, &state->permArena);
	state->explosionEmitter  = InitEmitter(explosionAnimDuration, 16, &state->permArena);
	state->shellTrailEmitter = InitEmitter(explosionAnimDuration * 1.5f, 16, &state->permArena);
	state->impactEmitter     = InitEmitter(10.0f, 32, &state->permArena);

	state->random = {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL};
	state->cameraZoom = 1.25f;
	state->ui = (UILayout*)ArenaPush(&state->permArena, sizeof(UILayout));
	state->props = (TransformHierarchy*)ArenaPush(&state->permArena, sizeof(TransformHierarchy));
	state->instances = ArrayInit(sizeof(PrefabInstance), 256, ArenaPush(&state->permArena, sizeof(PrefabInstance) * 256));

	Array prefabData = ParsePrefabInstancesCSV("prefab_instances.csv", &state->frameArena);
	for (int i = 0; i < prefabData.count; i++)
	{
		PrefabInstanceData data = *((PrefabInstanceData*)prefabData.elements + i);
		PrefabInstance instance = CreatePrefabInstance(data, NULL, state->props);
		ArrayPush(&state->instances, &instance);
	}
}

void ClientStop(GameState * state, PlatformAPI * platform)
{
	if (state->connected == false) { return; }

	memset(state->tanks, 0, sizeof(state->tanks));
	state->clientState = CLIENT_STATE_MAIN_MENU;
	state->welcomeReceived = false;
	state->helloSent = false;
	state->connected = false;
	platform->stopClient();
}

void ClientProcessWelcomePacket(GameState * state, WelcomePacket * packet, u32 connID)
{
	state->playerID = packet->playerID;
	for (int i = 0; i < packet->playerCount; i++)
	{
		PlayerConnectData * connectData = &packet->playerData[i];
		TankGFX * tank = &state->tanks[connectData->playerID];
		tank->active = true;
		tank->playerID = connectData->playerID;
		tank->style = connectData->style;
		copy_c_str(tank->displayName, connectData->displayName, sizeof(tank->displayName));
	}
	state->welcomeReceived = true;
}

void ClientProcessConnectPacket(GameState * state, ConnectPacket * packet)
{
	TankGFX * tank = &state->tanks[packet->playerData.playerID];
	tank->active = true;
	tank->playerID = packet->playerData.playerID;
	tank->style = packet->playerData.style;
	copy_c_str(tank->displayName, packet->playerData.displayName, sizeof(tank->displayName));
}

void ClientProcessDisconnectPacket(GameState * state, DisconnectPacket * packet)
{
	TankGFX * tank = &state->tanks[packet->playerID];
	tank->active = false;
}

void ClientProcessUpdatePacket(GameState * state, UpdatePacket * packet)
{
	for (int i = 0; i < packet->count; i++)
	{
		PlayerUpdateData * updateData = packet->playerData + i;

		TankGFX * player = &state->tanks[updateData->playerID];
		player->health = updateData->health;
		player->position = updateData->pos;
		player->turretRot = updateData->turretRot;
		player->rotation = updateData->rotation;
		player->kills = updateData->kills;
	}
}

void ClientProcessPlayerFiredPacket(GameState * state, PlayerFiredPacket * packet)
{
	TankPlayFireEffects(packet->playerID, packet->hitPosition, state);
}

void ClientHandlePacket(GameState * state, NetworkPacket * packet)
{
	PacketType type = (PacketType)(((u8*)packet->data)[0]);
	ReadStream stream((u8*)packet->data, packet->size);

	switch (type)
	{
		case PACKET_TYPE_WELCOME:
		{
			WelcomePacket welcomePkt;
			if (welcomePkt.serialize(stream))
			{
				ClientProcessWelcomePacket(state, &welcomePkt, packet->id);
			}
		} break;
		case PACKET_TYPE_CONNECT:
		{
			ConnectPacket connectPkt;
			if (connectPkt.serialize(stream))
			{
				ClientProcessConnectPacket(state, &connectPkt);
			}
		} break;
		case PACKET_TYPE_UPDATE:
		{
			UpdatePacket updatePkt;
			if (updatePkt.serialize(stream, &state->frameArena))
			{
				ClientProcessUpdatePacket(state, &updatePkt);
			}
		} break;
		case PACKET_TYPE_FIRED:
		{
			PlayerFiredPacket firedPkt;
			if (firedPkt.serialize(stream))
			{
				ClientProcessPlayerFiredPacket(state, &firedPkt);
			}
		} break;
		case PACKET_TYPE_DISCONNECT:
		{
			DisconnectPacket disconnectPkt;
			if (disconnectPkt.serialize(stream))
			{
				ClientProcessDisconnectPacket(state, &disconnectPkt);
			}
		}break;
		default: break;
	}
}

void ClientSendInput(GameState * state, GameInput * input, PlatformAPI * platform, vec2 mouseWorld)
{
	if (state->time - state->timeSinceLastUpdate < 0.016) { return; }
	state->timeSinceLastUpdate = state->time;

	TankGFX * localPlayer = &state->tanks[state->playerID];
	localPlayer->turretRot = Vec2AngleToRad(localPlayer->position, mouseWorld);

	InputPacket pkt = {0};
	pkt.turretRot = localPlayer->turretRot;

	if (input->WASD[0].isDown) {pkt.input |= 1 << 0; };
	if (input->WASD[1].isDown) {pkt.input |= 1 << 1; };
	if (input->WASD[2].isDown) {pkt.input |= 1 << 2; };
	if (input->WASD[3].isDown) {pkt.input |= 1 << 3; };
	if (input->isSpacePressed) {pkt.input |= 1 << 4; };

	ScratchArena temp(&state->frameArena);
	WriteStream stream = {(u8*)ArenaPush(temp.arena, KB(1)), KB(1)};
	if (pkt.serialize(stream))
	{
		platform->platformClientSend(stream.buffer, stream.pos, 0);
	}
}

void DrawUI(UILayout & ui, RendererPushBuffer * renderCMDs)
{
	for (int i = 1; i < ui.count; i++)
	{
		UINode * node = &ui.nodes[i];
		switch (node->data.type)
		{
			case UINodeType::UI_NODE_TYPE_CONTAINER:
			{
				if (node->data.container.visible)
				{
					RendererPushSDFRect(renderCMDs, node->pos, node->size, &node->data.container.style, 1);
				}
			} break;
			case UINodeType::UI_NODE_TYPE_TEXT:
			{
				vec4 fillColor = {node->data.text.fillColor.r, node->data.text.fillColor.g, node->data.text.fillColor.b, 1.0f};
				TextStyle style = {.fillColor = fillColor, .strokeWidth = node->data.text.strokeWidth};
				RendererPushText(renderCMDs, node->data.text.text, node->data.text.fontSize, node->data.text.fontHandle, node->pos, style, false, 30);
			} break;
			case UINodeType::UI_NODE_TYPE_IMAGE:
			{
				vec2 imagePos = {node->pos.x + node->size.x / 2.0f, node->pos.y + node->size.y / 2.0f};
				vec2 imageSize = {node->size.x*2, -node->size.y*2};
				if (node->data.image.uv == vec4::zero)
				{
					mat4 imgModel = ModelMatrix2D(imagePos, 0.0f, imageSize);
					RendererPushImage(renderCMDs, node->data.image.handle, 1.0f, imgModel, 30);
				}
				else // Atlas Image
				{
					RendererPushSubTexture(renderCMDs, node->data.image.handle, {imagePos.x, imagePos.y, 0.0f}, 0.0f, imageSize, node->data.image.uv, 31);
				}
			} break;
			case UINodeType::UI_NODE_TYPE_BUTTON:
			{

				SDFShapeStyle style;
				style.cornerRadius = node->data.button.style->cornerRadius;
				style.strokeWidth  = node->data.button.style->strokeWidth;
				if (node->id == ui.active)
				{
					style.fillColor = node->data.button.style->pressed.fillColor;
					style.strokeColor = node->data.button.style->pressed.strokeColor;
				}
				else if (node->id == ui.hot)
				{
					style.fillColor = node->data.button.style->hovered.fillColor;
					style.strokeColor = node->data.button.style->hovered.strokeColor;
				}
				else
				{
					style.fillColor = node->data.button.style->normal.fillColor;
					style.strokeColor = node->data.button.style->normal.strokeColor;
				}
				if (node->data.button.style->isTriangle)
				{
					RendererPushSDFTriangle(renderCMDs, node->pos, node->data.button.style->rotation, node->size, &style, 1);
				}
				else
				{
					RendererPushSDFRect(renderCMDs, node->pos, node->size, &style, 1);
				}
			}break;
			case UINodeType::UI_NODE_TYPE_INPUT_FIELD:
			{
				SDFShapeStyle style;
				style.cornerRadius = node->data.input.style->cornerRadius;
				style.strokeWidth  = node->data.input.style->strokeWidth;
				if (node->id == ui.active)
				{
					style.fillColor = node->data.input.style->pressed.fillColor;
					style.strokeColor = node->data.input.style->pressed.strokeColor;
				}
				else if (node->id == ui.hot)
				{
					style.fillColor = node->data.input.style->hovered.fillColor;
					style.strokeColor = node->data.input.style->hovered.strokeColor;
				}
				else
				{
					style.fillColor = node->data.input.style->normal.fillColor;
					style.strokeColor = node->data.input.style->normal.strokeColor;
				}
				RendererPushSDFRect(renderCMDs, node->pos, node->size, &style, 1);
			} break;
			default:
			{

			} break;
		}
	}

	ui.hot = 0;
	ui.depth = 0;
	ui.count = 0;
}

ShapeColor BUTTON_STYLE = {.fillColor    = ColorHexToRBGANormalized(0x262D33CC),
					       .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};

ShapeColor CUSTOMIZE_BTN_COLOR = {.fillColor    = ColorHexToRBGANormalized(0xFFFFFFFF),
								  .strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};

ShapeColor CUSTOMIZE_BTN_HOVERED = {.fillColor    = ColorHexToRBGANormalized(0xCACACAFF),
									.strokeColor  = ColorHexToRBGANormalized(0x262D33FF)};

ShapeColor BUTTON_HOVERED_COLOR = {.fillColor   = ColorHexToRBGANormalized(0x1E2328CC),
							       .strokeColor = ColorHexToRBGANormalized(0x262D33FF)};

ShapeColor BUTTON_PRESSED_COLOR = {.fillColor    = ColorHexToRBGANormalized(0x262D33CC),
								   .strokeColor  = ColorHexToRBGANormalized(0xFF00C8FF)};

ButtonStyle DEFAULT_BUTTON_STYLE = {BUTTON_STYLE, BUTTON_HOVERED_COLOR, BUTTON_PRESSED_COLOR, 6, 3};

ButtonStyle CUSTOMIZE_BUTTON_STYLE_R = {CUSTOMIZE_BTN_COLOR, CUSTOMIZE_BTN_HOVERED, BUTTON_PRESSED_COLOR, 6, 3, true, 0.523599};
ButtonStyle CUSTOMIZE_BUTTON_STYLE_L = {CUSTOMIZE_BTN_COLOR, CUSTOMIZE_BTN_HOVERED, BUTTON_PRESSED_COLOR, 6, 3, true,-0.523599};

SDFShapeStyle DEFAULT_CONTAINER_STYLE = {BUTTON_STYLE.fillColor, BUTTON_STYLE.strokeColor, 6, 3};

void UITank(UILayout & ui, TankStyle style, f32 scale, AtlasEntry * tankAtlasEntries, i32 tankAtlasHandle)
{
	UINodeLayout tankImageContainer = {.size = {640 * scale, 800 * scale}, .sizing = SizingType::FIXED};
	ui.begin("TANK_IMAGE_CONTAINER", tankImageContainer);
		vec4 tankUVs[3];
		CalculateTankUVs(style, tankAtlasEntries, &tankUVs[0]);
		ui.image("TANK_TRACK_IMAGE",  640 * scale, 800 * scale, tankAtlasHandle, {0, 0},  tankUVs[0]);
		ui.image("TANK_BODY_IMAGE",   512 * scale, 800 * scale, tankAtlasHandle, {65 * scale, 0}, tankUVs[1]);
		ui.image("TANK_TURRET_IMAGE", 456 * scale, 800 * scale, tankAtlasHandle, {92 * scale, -50 * scale}, tankUVs[2]);
	ui.end();
}

void ErrorPopupUI(UILayout & ui, u32 fontHandle, const char * message)
{
	UINodeData popupContainerData = {.type = UINodeType::UI_NODE_TYPE_CONTAINER, .container = { .visible = true, .style = POPUP_CONTAINER_STYLE}};
	UINodeLayout popupContainerLayout = {.childGap = 16.0f,
										 .justify = LayoutType::CENTER,
										 .align = LayoutType::CENTER,
										 .sizingY = SizingType::HUG,
										 .padding = {32,16,16,16},
										 .data = popupContainerData};
	ui.begin("PLAYER_CARD", popupContainerLayout);
		ui.text("PLAYER_DISPLAY_NAME_TEXT", fontHandle, 56.0f, ColorHexToRBGNormalized(0xFF3838), "!");
		ui.text("PLAYER_DISPLAY_NAME_TEXT", fontHandle, 30.0f, ColorHexToRBGNormalized(0xFF3838), message);
	ui.end();
}

void MainMenu(GameState * state, GameMemory * gameMemory, GameInput * input, PlatformAPI * platform)
{
	FontStyle fontStyle = {state->interFontHandle, 40.0f, 0.0f};

	UINodeData menuContainerData = {.type = UINodeType::UI_NODE_TYPE_CONTAINER, .container = { .visible = true, .style = DEFAULT_CONTAINER_STYLE}};
	f32 fontSize = 40.0f;
	UIPadding defaultPadding = {18.0f, 18.0f, 18.0f, 18.0f};
	UINodeLayout profileLayout = {.size = {0.0, 0.0f},
								  .childGap = 12.0f,
								  .axis = LayoutDirection::LEFT_TO_RIGHT,
								  .justify = LayoutType::END,
								  .align = LayoutType::START,
								  .sizing = SizingType::FILL,
								  .sizingY = SizingType::HUG,
								  .padding = {.right = 25.0f, .bottom = 20.0f}};

	UINodeLayout optionContainerLayout = {.childGap = 30.0f,
										  .axis = LayoutDirection::TOP_TO_BOTTOM,
										  .sizing = SizingType::HUG,
										  .padding = {50.0f,50.0f,25.0f,25.0f}};

	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL};
	UILayout & ui = *state->ui;
	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	ui.startLayout(frameStartSize, LayoutType::SPACE_BETWEEN, LayoutType::CENTER, platform->measureText, platform->copyClipboardText, uiInput);
		ui.begin("EMPTY", {.size = {(f32)input->viewportSize.x, 120.0f}}); ui.end();
		ui.begin("MENU_OPTION_CONTAINER", optionContainerLayout);
			ui.button("HOST_BUTTON", {390.0f, 100.0f}, &DEFAULT_BUTTON_STYLE, "HOST GAME", &fontStyle);
			ui.button("JOIN_BUTTON", {390.0f, 100.0f}, &DEFAULT_BUTTON_STYLE, "JOIN GAME", &fontStyle);
		ui.end();
		ui.begin("PROFILE_MENU_CONTAINER", profileLayout);
			ui.begin("PLAYER_CARD", {.childGap = 12.0f, .justify = LayoutType::CENTER, .align = LayoutType::CENTER, .sizingY = SizingType::FILL, .padding = {25,25,0,4},.data = menuContainerData});
				ui.text("PLAYER_DISPLAY_NAME_TEXT", state->interFontHandle, 30.0f, 0.0f, state->displayName);
				UITank(ui, state->playerStyle, 0.11f, state->tankAtlasEntries, state->tankAtlasHandle);
			ui.end();
			ui.begin_button("CUSTOMIZE_BUTTON", {100.0f, 100.0f}, &DEFAULT_BUTTON_STYLE);
				ui.image("CUSTOMIZE_ICON", 60, 60, state->customizeIconTextureHandle);
			ui.end();
		ui.end();
	ui.endLayout();

	if (ui.isButtonPressed("HOST_BUTTON"))
	{
		copy_c_str(state->serverIP, "::1", 64);
		ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));
		ServerStart(serverState, 7777, 8);
		platform->platformStartClient(state->serverIP, 7777);
		state->clientState = CLIENT_STATE_CONNECTING;
	}

	if (ui.isButtonPressed("JOIN_BUTTON"))
	{
		state->clientState = CLIENT_STATE_JOIN_MENU;
	}

	if (ui.isButtonPressed("CUSTOMIZE_BUTTON"))
	{
		state->clientState = CLIENT_STATE_CUSTOMIZE_MENU;
	}
}

bool ClientSendHelloMessage(GameState * state, PlatformAPI * platform)
{
	u8 buf[64] = {0};
	WriteStream stream(buf, sizeof(buf));
	HelloPacket packet = {state->playerStyle};
	copy_c_str(packet.displayName, state->displayName, sizeof(packet.displayName));

	if (packet.serialize(stream))
	{
		platform->platformClientSend(stream.buffer, stream.pos, 1);
		return true;
	}
	else
	{
		return false;
	}
}

void ConnectingScreen(GameState * state, GameInput * input, PlatformAPI * platform)
{
	static double nextSwapTime = 0;
	static u32 currentText = 0;
	const char * texts[] = {"Connecting.", "Connecting..", "Connecting..."};
	if (state->time > nextSwapTime)
	{
		nextSwapTime = state->time + 0.3f;
		currentText = (currentText + 1) % 3;
	}

	if (state->connected)
	{
		if (state->helloSent)
		{
			if (state->welcomeReceived)
			{
				state->clientState = CLIENT_STATE_CONNECTED;
			}
		}
		else // Send hello message
		{
			ClientSendHelloMessage(state, platform);
			state->helloSent = true;
		}
	}

	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL};
	uiInput.charsPressed = input->charsPressed;
	uiInput.charCount = input->charCount;
	UILayout & ui = *state->ui;

	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	ui.startLayout(frameStartSize, LayoutType::CENTER, LayoutType::CENTER, platform->measureText, platform->copyClipboardText, uiInput);
		ui.text("CONNECTING_TEXT", state->interFontHandle, 48.0f, 0.2f, texts[currentText]);
	ui.endLayout();
}

void DirectJoinMenu(GameState * state, GameInput * input, PlatformAPI * platform)
{
	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL, input->CTRL_V, input->charsPressed, input->charCount};
	UILayout & ui = *state->ui;
	UINodeLayout sectionLayout = {.childGap = 6.0f,
							      .axis = LayoutDirection::TOP_TO_BOTTOM,
								  .sizing = SizingType::HUG};
	UINodeLayout profileLayout = {.size = {0.0, 120.0f},
								  .axis = LayoutDirection::LEFT_TO_RIGHT,
								  .align = LayoutType::START,
								  .sizing = SizingType::FILL,
								  .sizingY = SizingType::FIXED,
								  .padding = {.left = 16.0f, .top = 16.0f}};


	FontStyle btnFontStyle = {state->interFontHandle, 40.0f, 0.0f};
	FontStyle ipInputFontStyle = {state->interFontHandle, 18.0f, 0.0f};
	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	ui.startLayout(frameStartSize, LayoutType::SPACE_BETWEEN, LayoutType::CENTER, platform->measureText, platform->copyClipboardText, uiInput);
		ui.begin("POPUP_CONTAINER", profileLayout);
			if (state->showBadIPPopup) { ErrorPopupUI(ui, state->interFontHandle, "invalid IP entered"); }
			if (state->showConnFailedPopup) { ErrorPopupUI(ui, state->interFontHandle, "failed to connect to server"); }
		ui.end();
		ui.begin("IP_SECTION", sectionLayout);
			ui.text("IP_LABEL", state->interFontHandle, 30.0f, 0.2f, "IPv6 Address:");
			ui.inputField("IP_INPUT", {428.0f, 70.0f},
						  state->serverIP, 40,
						  &DEFAULT_BUTTON_STYLE,ipInputFontStyle,
						  UI_INPUT_ALLOW_ALPHANUM | UI_INPUT_ALLOW_COLONS);
			ui.button("CONNECT_BTN",{428,80.f}, &DEFAULT_BUTTON_STYLE, "CONNECT", &btnFontStyle);
			ui.button("BACK_BTN",{150,60}, &DEFAULT_BUTTON_STYLE, "BACK", &btnFontStyle);
		ui.end();
		ui.begin("EMPTY", {.size = {(f32)input->viewportSize.x, 120.0f}}); ui.end();
	ui.endLayout();

	if (ui.isButtonPressed("CONNECT_BTN"))
	{
		if (platform->platformStartClient(state->serverIP, 7777))
		{
			state->clientState = CLIENT_STATE_CONNECTING;
			state->showBadIPPopup = false;
		}
		else
		{
			state->showBadIPPopup = true;
		}
		
	}
	if (ui.isButtonPressed("BACK_BTN"))
	{
		state->clientState = CLIENT_STATE_MAIN_MENU;
		state->showBadIPPopup = false;
		state->showConnFailedPopup = false;
	}

	if (ui.inputEntered("IP_INPUT"))
	{

	}
}

void PlayerCard(UILayout & ui, GameState * state, TankGFX * player)
{
	UINodeLayout cardLayout = {.childGap = 12.0f,
							   .axis = LayoutDirection::LEFT_TO_RIGHT,
								  .justify = LayoutType::START,
								  .align = LayoutType::CENTER,
								  .sizing = SizingType::HUG,
								  .sizingY = SizingType::HUG};
	UINodeLayout nameKillsLayout = {.childGap = 4,
									.axis = LayoutDirection::TOP_TO_BOTTOM,
									.justify = LayoutType::START,
									.align = LayoutType::START,
									.sizing = SizingType::HUG,
									.sizingY = SizingType::HUG};

	char killsText[32];
	snprintf(killsText, sizeof(killsText), "%d", player->kills); 
	ui.begin("CARD_CONTAINER", cardLayout);
		UITank(ui, player->style, 0.12f, state->tankAtlasEntries, state->tankAtlasHandle);
		ui.begin("NAME_KILLS_CONTAINER", nameKillsLayout);
			ui.text("PLAYER_CARD_DISPLAY_NAME", state->interFontHandle, 24.0f, 0.2f, player->displayName);
			ui.begin("KILLS_CONTAINER", {.childGap = 6.0f, .axis = LayoutDirection::LEFT_TO_RIGHT, .align = LayoutType::CENTER});
				ui.image("TARGET_ICON_IMAGE", 24, 24, state->targetIconTextureHandle);
				ui.text("PLAYER_CARD_KILLS", state->interFontHandle, 24.0f, 0.0f, killsText);
			ui.end();
		ui.end();
	ui.end();
}

void EscapeMenu(GameState * state, GameInput * input, GameMemory * gameMemory, PlatformAPI * platform, RendererPushBuffer * renderCMDs)
{
	if (!state->optionsMenuOpen) { return; }
	UILayout & ui = *state->ui;
	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL, input->CTRL_V,input->charsPressed, input->charCount};
	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	FontStyle fontStyle = {state->interFontHandle, 40.0f, 0.0f};

	UINodeLayout optionContainerLayout = {.childGap = 30.0f,
										  .axis = LayoutDirection::TOP_TO_BOTTOM,
										  .sizing = SizingType::HUG,
										  .padding = {50.0f,50.0f,25.0f,25.0f}};

	ui.startLayout(frameStartSize, LayoutType::CENTER, LayoutType::CENTER, platform->measureText, platform->copyClipboardText, uiInput);
		ui.begin("MENU_OPTION_CONTAINER", optionContainerLayout);
			ui.button("QUIT_BTN", {390.0f, 100.0f}, &DEFAULT_BUTTON_STYLE, "QUIT", &fontStyle);
			ui.button("RESUME_BTN", {390.0f, 100.0f}, &DEFAULT_BUTTON_STYLE, "RESUME", &fontStyle);
		ui.end();
	ui.endLayout();

	if (ui.isButtonPressed("RESUME_BTN"))
	{
		state->optionsMenuOpen = false;
	}
	if (ui.isButtonPressed("QUIT_BTN"))
	{
		state->optionsMenuOpen = false;
		ClientStop(state, platform);
		ServerState * serverState = (ServerState*)((u8*)gameMemory->permStorage + sizeof(GameState));
		ServerStop(serverState);
	}
	mat4 uiProjection = orthographicProjection(input->viewportSize.x, 0.0f, 0.0f, input->viewportSize.y, -1.0f, 100.f);
	RendererPushSetProjection(renderCMDs, uiProjection, uiProjection);
	DrawUI(*state->ui, renderCMDs);
}

void GameHUD(GameState * state, GameInput * input, PlatformAPI * platform)
{
	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL};
	uiInput.charsPressed = input->charsPressed;
	uiInput.charCount = input->charCount;
	UILayout & ui = *state->ui;

	UINodeLayout cardContainerLayout = {.childGap = 12.0f,
							            .axis = LayoutDirection::TOP_TO_BOTTOM,
										.justify = LayoutType::START,
									    .align = LayoutType::START,
										.padding = {}};
	
	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	ui.startLayout(frameStartSize, LayoutType::START, LayoutType::START, platform->measureText, platform->copyClipboardText, uiInput);
		ui.begin("CONTAINER_I_HARDLY_KNOW_HER", {.size = {0,0.0f}, .axis = LayoutDirection::LEFT_TO_RIGHT,
												 .justify = LayoutType::SPACE_BETWEEN,
												 .sizing = SizingType::FILL,
												 .padding = {.left = 16, .right = 16, .top = 16.0f}});
			ui.begin("PLAYER_CARDS_CONTAINER", cardContainerLayout);
				for (int i = 0; i < MAX_PLAYERS; i++)
				{
					TankGFX * player = &state->tanks[i];
					if (player->active)
					{
						PlayerCard(ui, state, player);
					}
				}
			ui.end();

			ui.begin_button("OPTIONS_BUTTON", {75.0f, 75.0f}, &DEFAULT_BUTTON_STYLE);
				ui.image("HAMBURGER_ICON", 42, 28, state->hamburgerIconTextureHandle);
			ui.end();
		ui.end();

	ui.endLayout();

	if (ui.isButtonPressed("OPTIONS_BUTTON")) { state->optionsMenuOpen = !state->optionsMenuOpen; }
}

void CustomizeMenu(GameState * state, GameInput * input, PlatformAPI * platform)
{
	UIInput uiInput = {{(f32)input->mousePosVP.x, (f32)input->mousePosVP.y}, input->mouseL};
	uiInput.charsPressed = input->charsPressed;
	uiInput.charCount = input->charCount;
	UILayout & ui = *state->ui;

	UINodeData menuContainerData = {.type = UINodeType::UI_NODE_TYPE_CONTAINER, .container = { .visible = true, .style = DEFAULT_CONTAINER_STYLE}};
	UINodeLayout sectionContainerLayout = {.childGap = 12.0f,
										   .axis = LayoutDirection::TOP_TO_BOTTOM,
										   .sizing = SizingType::HUG,
										   .sizingY = SizingType::HUG};
	UINodeLayout sectionLayout = {.childGap = 6.0f,
								.axis = LayoutDirection::TOP_TO_BOTTOM,
									 .sizing = SizingType::FILL,
									 .sizingY = SizingType::HUG};
	UINodeLayout doneSectionLayout = {.childGap = 6.0f,
									.axis = LayoutDirection::TOP_TO_BOTTOM,
									.align = LayoutType::END,
									 .sizing = SizingType::FILL,
									 .sizingY = SizingType::HUG,
	};

	UINodeLayout containerLeft = {.childGap = 16.0f,
								  .axis = LayoutDirection::LEFT_TO_RIGHT,
								  .sizing = SizingType::HUG};
	UINodeLayout colorBtnContainer = {.childGap = 10.0f,
									  .axis = LayoutDirection::LEFT_TO_RIGHT,
									  .sizing = SizingType::FILL,
									  .sizingY = SizingType::HUG,
									  .padding = {25,25,16,16},
									  .data = menuContainerData};

	UINodeLayout tankImageContainer = {.size = {640 * 0.33, 220}, .sizing = SizingType::FIXED};

	UINodeLayout optionContainerLayout = {.axis = LayoutDirection::LEFT_TO_RIGHT,
										  .align = LayoutType::CENTER,
										  .sizing = SizingType::HUG,
										  .padding = {16.0f,16.0f,25.0f,25.0f},
										  .data = menuContainerData};
	UINodeLayout bodyButtonLayout = {.childGap = 16.0f,
									 .axis = LayoutDirection::TOP_TO_BOTTOM,
									 .sizing = SizingType::HUG};

	FontStyle btnFontStyle = {state->interFontHandle, 40.0f, 0.0f};

	FontStyle nameCharCountFontStyle = {state->interFontHandle, 30.0f, 0.0f, ColorHexToRBGNormalized(0x827A5D)};
	vec2 customizeBtnSize = {60.0f,60.0f};
	vec2 colorBtnSize = {50.0f, 50.0f};
	vec2 frameStartSize = {(f32)input->viewportSize.x, (f32)input->viewportSize.y};
	ui.startLayout(frameStartSize, LayoutType::CENTER, LayoutType::CENTER, platform->measureText, platform->copyClipboardText, uiInput);
		ui.begin("SECTION_CONTAINER", sectionContainerLayout);
			ui.begin("DISPLAY_NAME_SECTION", sectionLayout);
				ui.begin("INPUT_PROMPT_CONTAINER", containerLeft);
					ui.text("DISPLAY_NAME_LABEL", state->interFontHandle, 30.0f, 0.2f, "Display Name:");

					char buf[32];
					snprintf(buf, sizeof(buf), "(%d/20)", (i32)strnlen(state->displayName, 32));
					ui.text("DISPLAY_NAME_LENGTH", nameCharCountFontStyle, buf);
				ui.end();
				ui.inputField("DISPLAY_NAME_INPUT", {360.0f, 70.0f}, state->displayName, 32, &DEFAULT_BUTTON_STYLE, btnFontStyle,
																		UI_INPUT_ALLOW_ALPHANUM | UI_INPUT_ALLOW_UNDERSCORES);
			ui.end();
			ui.begin("TANK_STYLE_SECTION", sectionLayout);
				ui.text("STYLE_LABEL", state->interFontHandle, 30.0f, 0.2f, "Tank Body:");
				ui.begin("BODY_STYLE_OPTIONS_CONTAINER", optionContainerLayout);
					ui.begin("LEFT_STYLE_BUTTONS", bodyButtonLayout);
						ui.button("STYLE_BTN_TURRET_L", customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_L);
						ui.button("STYLE_BTN_BODY_L",   customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_L);
						ui.button("STYLE_BTN_TRACK_L",  customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_L);
					ui.end();
					UITank(ui, state->playerStyle, 0.33f, state->tankAtlasEntries, state->tankAtlasHandle);
					ui.begin("RIGHT_STYLE_BUTTONS", bodyButtonLayout);
						ui.button("STYLE_BTN_TURRET_R", customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_R);
						ui.button("STYLE_BTN_BODY_R",   customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_R);
						ui.button("STYLE_BTN_TRACK_R",  customizeBtnSize, &CUSTOMIZE_BUTTON_STYLE_R);
					ui.end();
				ui.end();
			ui.end();
			ui.begin("COLOR_SECTION", sectionLayout);
				ui.text("COLOR_LABEL", state->interFontHandle, 30.0f, 0.2f, "Tank Color:");
					ui.begin("LEFT_STYLE_BUTTONS", colorBtnContainer);
						ui.button("COLOR_BTN_GREEN",     colorBtnSize, &BTN_GREEN_STYLE);
						ui.button("COLOR_BTN_GREY",      colorBtnSize, &BTN_GREY_STYLE);
						ui.button("COLOR_BTN_DARK_GREY", colorBtnSize, &BTN_DARK_GREY_STYLE);
						ui.button("COLOR_BTN_TAN",       colorBtnSize, &BTN_TAN_STYLE);
					ui.end();
			ui.end();
			ui.begin("DONE_SECTION", doneSectionLayout);
				ui.button("BACK_BTN",{200,75.f}, &DEFAULT_BUTTON_STYLE, "DONE", &btnFontStyle);
			ui.end();
		ui.end();
	ui.endLayout();

	if (ui.isButtonPressed("STYLE_BTN_TURRET_R")) { state->playerStyle.turretType = (state->playerStyle.turretType  + 1) % 3; }
	if (ui.isButtonPressed("STYLE_BTN_TRACK_R")) { state->playerStyle.trackType = (state->playerStyle.trackType + 1) % 3; }
	if (ui.isButtonPressed("STYLE_BTN_BODY_R")) { state->playerStyle.bodyType = (state->playerStyle.bodyType + 1) % 3; }

	if (ui.isButtonPressed("STYLE_BTN_TURRET_L")) { state->playerStyle.turretType = (state->playerStyle.turretType + 2) % 3; }
	if (ui.isButtonPressed("STYLE_BTN_TRACK_L")) { state->playerStyle.trackType = (state->playerStyle.trackType + 2) % 3; }
	if (ui.isButtonPressed("STYLE_BTN_BODY_L")) { state->playerStyle.bodyType = (state->playerStyle.bodyType + 2) % 3; }

	if (ui.isButtonPressed("COLOR_BTN_DARK_GREY")) { state->playerStyle.colorID = 0; }
	if (ui.isButtonPressed("COLOR_BTN_GREEN"))     { state->playerStyle.colorID = 1; }
	if (ui.isButtonPressed("COLOR_BTN_GREY"))      { state->playerStyle.colorID = 2; }
	if (ui.isButtonPressed("COLOR_BTN_TAN"))       { state->playerStyle.colorID = 3; }

	if (ui.inputEntered("DISPLAY_NAME_INPUT"))
	{

	}

	if (ui.isButtonPressed("BACK_BTN")) { state->clientState = CLIENT_STATE_MAIN_MENU; }
}

void DrawScrollingBackground(GameState * state, GameInput * input, RendererPushBuffer * renderCommands)
{
	vec4 clearColor = ColorHexToRBGANormalized(0xC3B67CFF);
	RendererPushSetClear(renderCommands, clearColor);

	float aspect = (float)input->viewportSize.x / (float)input->viewportSize.y;

	mat4 view = translationMatrix(-state->cameraPos.x, -state->cameraPos.y, -1.0f);
	mat4 projection = orthographicProjection(aspect*state->cameraZoom,
											-aspect*state->cameraZoom,
											1.0f*state->cameraZoom,
											-1.0f*state->cameraZoom, -0.01f, 100.0f);
	mat4 VP = transpose(projection * view);
	RendererPushSetProjection(renderCommands, VP, projection);
	
	float halfWidth  = aspect * state->cameraZoom;
	float halfHeight = 1.0f * state->cameraZoom;
	vec2 size = {halfWidth * 2, halfHeight * 2};
	float tileWorldWidth  = 1.5f;
	float tileWorldHeight = tileWorldWidth * (9.0f / 16.0f); // preserve 16:9
	vec2 uvOffset = { state->cameraPos.x / tileWorldWidth, -state->cameraPos.y / tileWorldHeight };
	vec2 tilingAmount = { size.x / tileWorldWidth, size.y / tileWorldHeight };
	RendererPushScrollingTexture(renderCommands, state->desertBackgroundTextureHandle, {-halfWidth, -halfHeight}, size, uvOffset, tilingAmount, 0);
}

void DrawPrefab(const PrefabInstance * instance, GameState * state, RendererPushBuffer * renderCMDs)
{
	const Prefab * prefab = PREFABS[instance->prefabID];

	u32 textureHandle = 0;
	HashMapGet(&state->textureHandles, prefab->textureName, &textureHandle);

	mat4 prefabModel = ModelMatrix2D(prefab->position, prefab->rotation, prefab->scale);
	mat4 instanceModel = state->props->transforms[instance->transformIndex].world;
	RendererPushImage(renderCMDs, textureHandle, 1.0f, prefabModel * instanceModel, prefab->layer);
}

void DrawWorldBorder(RendererPushBuffer * renderCMDs, GameState * state)
{
	f32 offset = -0.16f;
	vec4 lineColor = {1.0f, 0.0f, 0.0f, 0.5f};
	vec2 corners[4] = {{ WORLD_EXTENTS.x - offset, WORLD_EXTENTS.z - offset}, // right, up
					   { WORLD_EXTENTS.y + offset, WORLD_EXTENTS.z - offset}, // left, up
			           { WORLD_EXTENTS.y + offset, WORLD_EXTENTS.w + offset }, // left, down
					   { WORLD_EXTENTS.x - offset, WORLD_EXTENTS.w + offset }}; // right, down

	for (int i = 0; i < 4; i++)
	{
		vec2 start = corners[i];
		vec2 end   = corners[(i + 1) % 4];
		RendererPushLine(renderCMDs, start, end, lineColor, 0.01f, 5);
	}

	f32 borderWidth = (fabs(corners[1].y) +  fabs(corners[0].x));
	f32 borderHeight = (fabs(corners[1].y) +  fabs(corners[2].y));
	f32 borderExtent = 3.0f;

	f32 uvOffset = state->time / 16.0f;
	RendererPushWorldBorder(renderCMDs, corners[1] + vec2{-borderExtent, 0.0f}, 		 {borderWidth + borderExtent * 2.0f, borderExtent}, 1.0f, uvOffset, 5);
	RendererPushWorldBorder(renderCMDs, corners[2] + vec2{-borderExtent, 0.0f}, 		 {3.0f, borderHeight}, 1.0f, uvOffset,5);
	RendererPushWorldBorder(renderCMDs, corners[2] + vec2{-borderExtent, -borderExtent}, {borderWidth + borderExtent * 2.0f, borderExtent}, 1.0f, uvOffset,5);
	RendererPushWorldBorder(renderCMDs, corners[3], {borderExtent, borderHeight}, 1.0f, uvOffset, 5);
}

void UpdateGame(GameState * state, GameInput * input, PlatformAPI * platform, RendererPushBuffer * renderCommands)
{
	float aspect = (float)input->viewportSize.x / (float)input->viewportSize.y;
	mat4 projection = orthographicProjection(aspect*state->cameraZoom,
											-aspect*state->cameraZoom,
											1.0f*state->cameraZoom,
											-1.0f*state->cameraZoom, -0.01f, 100.0f);
	vec2 mouseWorld = MousePosToWorld(input->mousePosVP, state->cameraPos, input->viewportSize, projection);


	ClientSendInput(state, input, platform, mouseWorld);

	mat4 gdEasy = ModelMatrix2D({0.75f, 0.75f}, 0.0f, {0.5f, 0.5f});
    RendererPushImage(renderCommands, 1, 1.0f, gdEasy, 1);

	SimulateParticles(&state->turretFireEmitter, input->deltaTime);
	SimulateParticles(&state->explosionEmitter,  input->deltaTime);
	SimulateParticles(&state->shellTrailEmitter, input->deltaTime);
	SimulateParticles(&state->impactEmitter,     input->deltaTime);
	DrawParticles(renderCommands, state);
	DrawWorldBorder(renderCommands, state);

	//RendererPushImage(renderCommands, state->airdropTextureHandle, {{TEST_}})

	SDFShapeStyle TRI_STYLE = {ColorHexToRBGANormalized(0x402525CC), ColorHexToRBGANormalized(0x332626FF), 16, 3};
	TankGFX * localPlayer = &state->tanks[state->playerID];
	state->cameraPos = vec2SmoothDamp(state->cameraPos, localPlayer->position, &state->cameraVelocity, 1.0f, 100.0f, input->deltaTime);

	f32 angle = fmodf(state->time, 2.0f * PI);

	state->props->UpdateTransforms();
	for (int i = 0; i < state->instances.count; i++)
	{
		PrefabInstance * instance = (PrefabInstance*)state->instances.elements + i;
		DrawPrefab(instance, state, renderCommands);
	}

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		UpdateTankGFX(&state->tanks[i], input->deltaTime);
		DrawTank(state->tanks[i], renderCommands, state);
	}

}
void ClientUpdate(GameState * state, GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands, RendererPushBuffer * uiRenderCMDs)
{
    state->time += input->deltaTime;
	ArenaClear(&state->frameArena);

	float aspect = (float)input->viewportSize.x / (float)input->viewportSize.y;
	mat4 projection = orthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f);

    double time = state->time;
    vec4 color = GetHSVSpectrumColor(time);

	for (int i = 0; i < input->clientEventCount; i++)
	{
		NetworkEvent * event = &input->clientEvents[i];
		switch (event->type) {
			case NET_EVENT_CLIENT_CONNECTED:
				state->connected = true;
				break;
			case NET_EVENT_CLIENT_DISCONNECTED:
				ClientStop(state, &gameMemory->platform);
				break;
			case NET_EVENT_PACKET:
				ClientHandlePacket(state, event->packet);
				break;
			case NET_EVENT_CLIENT_CONNECTION_FAILED:
				state->clientState = CLIENT_STATE_JOIN_MENU;
				state->showConnFailedPopup = true;
				state->showBadIPPopup = false;
				ClientStop(state, &gameMemory->platform);
				break;
			default:
				break;
		}
	}


	DrawScrollingBackground(state, input, renderCommands);

    char buf[256];
	snprintf(buf, sizeof(buf), "MouseVP: %d, %d", input->mousePosVP.x, input->mousePosVP.y);
   	const char * text = "This is some text!";
	vec2 textPos = {0.5,0};
	vec4 textColor = {1.0f, 0.0f, 0.0f, 1.0f};
	TextStyle testTextStyle = {.fillColor = textColor, .strokeWidth = 0.0f};
	f32 fontSize = 0.1f;
	RendererPushText(renderCommands, buf, fontSize, state->interFontHandle, textPos, testTextStyle, true, 30);

	switch (state->clientState)
	{
		case CLIENT_STATE_MAIN_MENU:
		{
			MainMenu(state, gameMemory, input, &gameMemory->platform);
		} break;
		case CLIENT_STATE_CUSTOMIZE_MENU:
		{
			CustomizeMenu(state, input, &gameMemory->platform);
		} break;
		case CLIENT_STATE_JOIN_MENU:
		{
			DirectJoinMenu(state, input, &gameMemory->platform);
		} break;
		case CLIENT_STATE_CONNECTING:
		{
			ConnectingScreen(state, input, &gameMemory->platform);
		} break;
		case CLIENT_STATE_CONNECTED:
		{
			UpdateGame(state, input, &gameMemory->platform, renderCommands);
			EscapeMenu(state, input, gameMemory, &gameMemory->platform, uiRenderCMDs);
			GameHUD(state, input, &gameMemory->platform);
		} break;
	}

	mat4 uiProjection = orthographicProjection(input->viewportSize.x, 0.0f, 0.0f, input->viewportSize.y, -1.0f, 100.f);
	RendererPushSetProjection(uiRenderCMDs, uiProjection, uiProjection);

	DrawUI(*state->ui, uiRenderCMDs);
}

