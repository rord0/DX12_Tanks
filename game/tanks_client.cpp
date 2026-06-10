#include "tanks_client.hpp"
#include "serialize.hpp"
#include "tanks.hpp"
#include "util.hpp"
#include "render_commands.hpp"
#include "ui.hpp"
#include "ui.cpp"
#include <cmath>
#include <cstddef>
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

vec4 CalculateUVTransform(AtlasEntry entry, u32 atlasHeight, u32 atlasWidth)
{
	vec4 uv = {(f32)entry.width / (f32)atlasWidth, (f32)entry.height / (f32)atlasHeight, (f32)entry.x / (f32)atlasWidth, (f32)entry.y / (f32)atlasHeight};
	return uv;
}

vec4 ColorHexToRBGANormalized(u32 color)
{
	float r = ((color >> 24) & 0xFF) / 255.0f;
    float g = ((color >> 16) & 0xFF) / 255.0f;
    float b = ((color >>  8) & 0xFF) / 255.0f;
    float a = ((color)       & 0xFF) / 255.0f;

    return vec4{r, g, b, a};
}

vec3 ColorHexToRBGNormalized(u32 color)
{
	float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8)  & 0xFF) / 255.0f;
    float b = ((color)       & 0xFF) / 255.0f;

    return vec3{r, g, b};
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

void DrawTank(TankGFX & tank, RendererPushBuffer * cmdBuffer, GameState * state)
{
	if (!tank.active) { return; }

    u32 trackIndex  =  0 + (3 * tank.style.trackType) + tank.style.colorID;
    u32 bodyIndex   = 12 + (3 * tank.style.bodyType ) + tank.style.colorID;
    u32 turretIndex = 24 + (3 * tank.style.trackType) + tank.style.colorID;

	//TODO(rordon): assert that indexes are less than 36.
	u32 atlasWidth  = 4096;
	u32 atlasHeight = 4096;

	vec4 trackUV  = CalculateUVTransform(state->tankAtlasEntries[trackIndex], atlasHeight, atlasWidth);
	vec4 bodyUV   = CalculateUVTransform(state->tankAtlasEntries[bodyIndex], atlasHeight, atlasWidth);
	vec4 turretUV = CalculateUVTransform(state->tankAtlasEntries[turretIndex], atlasHeight, atlasWidth);

	vec2 turretDir = vec2{cosf(tank.turretRot), sinf(tank.turretRot)};
	vec2 tankDir   = vec2{cosf(tank.rotation),  sinf(tank.rotation)};
	vec2 turretCenter = (-tankDir * -0.03f) + tank.position;
	vec2 turretPos = (-turretDir * 0.075f); // Offset backwards
	turretPos += (turretDir *  TurretRecoilOffset(tank.turretOffset)); // Add recoil offset.
	turretPos += turretCenter;

	f32 tankRot = tank.rotation + (PI/2.0f);

	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.8f, 1.0f}, trackUV, 0);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.64f, 1.0f}, bodyUV, 1);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {turretPos.x, turretPos.y, 0.0f}, tank.turretRot + (PI/2.0f), {0.57f, 1.0f}, turretUV, 2);

	DrawHealthbar(cmdBuffer, vec2{tank.position.x, tank.position.y + 0.3f}, ((float)tank.health / (float)TANK_MAX_HEALTH), tank.healthLerp);
	vec4 nameColor = {1.0f, 1.0f, 1.0f, 1.0f};
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

vec2 MousePosToWorld(vec2i mousePos, vec2i viewportSize, mat4 proj)
{
	float aspect = (float)viewportSize.x / (float)viewportSize.y;
	vec2 mouseNDC;
	mouseNDC.x = (2.0f * (mousePos.x / (float)viewportSize.x)) - 1.0f;
	mouseNDC.y = 1.0f - ((2.0f * mousePos.y) / (float)viewportSize.y);

	vec4 mouseClip = {mouseNDC.x, mouseNDC.y, 0.0f, 1.0f};
	vec4 worldPos = inverseOrthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f) * mouseClip;

	return {worldPos.x, worldPos.y};
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

void DEBUG_SyncTanks(Tank * tanks, TankGFX * tankGFX)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		tankGFX[i].playerID  = tanks[i].playerID;
		tankGFX[i].position  = tanks[i].position;
		tankGFX[i].rotation  = tanks[i].rotation;
		tankGFX[i].turretRot = tanks[i].turretRot;
		tankGFX[i].health	 = tanks[i].health;
		tankGFX[i].active	 = tanks[i].active;
		tankGFX[i].style	 = tanks[i].style;
	}
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

		InstanceData2D instanceData = {{effect->position.x, effect->position.y, 0.0f}, {0.6f, 0.6f}, angle, alpha};
		RendererPushImage(renderCmds, state->shellImpactTextureHandle, instanceData, 0);
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

void ClientStart(GameState * state, GameMemory * gameMemory)
{
	state->permArena = ArenaInit((u8*)gameMemory->permStorage + sizeof(GameState) + sizeof(ServerState),
								  gameMemory->permStorageSize - sizeof(GameState) - sizeof(ServerState));

    state->tankAtlasHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"tank_parts.png");
    state->extraTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"images/platformer/Props_AirDrop.png");
    state->shellImpactTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"shell_impact.png");
    state->customizeIconTextureHandle = gameMemory->platform.platformLoadTexture(RESOURCES_PATH"customize_icon.png");
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
		}
		default: break;
	}
}

void ClientSendInput(GameState * state, GameInput * input, GameMemory * memory, vec2 mouseWorld)
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
		memory->platform.platformClientSend(stream.buffer, stream.pos, 0);
	}
}

const char * HOST_BUTTON_TEXT = "HOST GAME";
const char * JOIN_BUTTON_TEXT = "JOIN GAME";

void DrawMainMenu(GameState * state, RendererPushBuffer * renderCMDs, PlatformAPI * platform, vec2i screen, vec2i mousePos)
{
	UILayout ui = {0};
	vec2 buttonSize = {390.0f, 100.0f};
	SDFShapeStyle buttonStyle = {.fillColor    = ColorHexToRBGANormalized(0x262D33CC),
							     .strokeColor  = ColorHexToRBGANormalized(0x262D33FF),
							     .cornerRadius = 6,
								 .strokeWidth  = 3};
	UINodeData menuContainerData = {.type = UINodeType::UI_NODE_TYPE_CONTAINER, .container = { .visible = true, .style = buttonStyle}};
	f32 fontSize = 40.0f;
	UIPadding defaultPadding = {18.0f, 18.0f, 18.0f, 18.0f};
	UINodeLayout profileLayout = {.size = {(f32)screen.x, 100.0f},
								  .childGap = 30.0f,
								  .axis = LayoutDirection::LEFT_TO_RIGHT,
								  .justify = LayoutType::END,
								  .align = LayoutType::CENTER,
								  .sizing = SizingType::FIXED,
								  .padding = {.right = 25.0f, .bottom = 25.0f}};

	UINodeLayout optionContainerLayout = {.childGap = 30.0f,
										  .axis = LayoutDirection::TOP_TO_BOTTOM,
										  .sizing = SizingType::HUG,
										  .padding = {50.0f,50.0f,25.0f,25.0f}};

	UINodeLayout buttonLayout = { .size = buttonSize,
								  .justify = LayoutType::CENTER,
								  .align = LayoutType::CENTER,
								  .sizing = SizingType::FIXED,
								  .data = {.type = UINodeType::UI_NODE_TYPE_CONTAINER, .container = {.visible = true, .style = buttonStyle}}
	};

	ui.startLayout(screen.x, screen.y, platform->measureText);
		ui.begin("EMPTY", screen.x, 120.0f, 30); ui.end();
		ui.begin("MENU_OPTION_CONTAINER", optionContainerLayout);
			ui.begin("HOST_BUTTON", buttonLayout);
				ui.text("HOST_TEXT", state->interFontHandle, fontSize, 0.0f, HOST_BUTTON_TEXT);
			ui.end();
			ui.begin("JOIN_BUTTON", buttonLayout);
				ui.text("JOIN_TEXT", state->interFontHandle, fontSize, 0.0f, JOIN_BUTTON_TEXT);
			ui.end();
		ui.end();
		ui.begin("PROFILE_MENU_CONTAINER", profileLayout);
			ui.begin("PLAYER_CARD", {.childGap = 18.0f, .justify = LayoutType::CENTER, .align = LayoutType::CENTER, .padding = defaultPadding, .data = menuContainerData});
				ui.text("PLAYER_DISPLAY_NAME_TEXT", state->interFontHandle, 30.0f, 0.0f, "LeekyBandz");
				ui.image("TANK", 64, 64, 1);
			ui.end();
			ui.begin("CUSTOMIZE_BUTTON", { .padding = defaultPadding,.data = menuContainerData});
				ui.image("CUSTOMIZE_ICON", 56, 56, state->customizeIconTextureHandle);
			ui.end();
		ui.end();
	ui.endLayout();

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
				TextStyle style = {.fillColor = {1.0f, 1.0f, 1.0f, 1.0f}, .strokeWidth = node->data.text.strokeWidth};
				RendererPushText(renderCMDs, node->data.text.text, node->data.text.fontSize, node->data.text.fontHandle, node->pos, style, false, 30);
			} break;
			case UINodeType::UI_NODE_TYPE_IMAGE:
			{
				InstanceData2D imgInstance = {{node->pos.x + node->size.x / 2.0f, node->pos.y + node->size.y / 2.0f, 0.0f}, {node->size.x*2, -node->size.y*2}, 0.0f, 1.0f};
				RendererPushImage(renderCMDs, node->data.image.handle, imgInstance, 30);
			} break;
			default:
			{

			} break;
		}
	}

}

void ClientUpdate(GameState * state, GameMemory * gameMemory, GameInput * input, RendererPushBuffer * renderCommands, RendererPushBuffer * uiRenderCMDs)
{
    state->time += input->deltaTime;
	ArenaClear(&state->frameArena);

	float aspect = (float)input->viewportSize.x / (float)input->viewportSize.y;
	mat4 projection = orthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f);

	vec2 cameraPos = {0.0f, 0.0f};
	mat4 view = translationMatrix(cameraPos.x, cameraPos.y, 1.0f);
	vec2 mouseWorld = MousePosToWorld(input->mousePosVP, input->viewportSize, projection);
    double time = state->time;
    vec4 color = GetHSVSpectrumColor(time);

    float angle = (float)fmod(time * 100, 360.0) * (PI/180.0f);
    float angle2 = (float)fmod(time * 10, 360.0) * (PI/180.0f);
    InstanceData2D gdHarder = {{0.0f, sinf(angle), 0.0f}, {0.8f, 1.0f}, angle};
    InstanceData2D gdHard   = {{gdHarder.position.x + sinf(angle) * -0.075f, gdHarder.position.y - cosf(angle) * -0.075f}, {0.57f, 1.0f}, angle};

	for (int i = 0; i < input->clientEventCount; i++)
	{
		NetworkEvent * event = &input->clientEvents[i];
		switch (event->type) {
			case NET_EVENT_CLIENT_CONNECTED:
				state->connected = true;
				break;
			case NET_EVENT_CLIENT_DISCONNECTED:
				state->connected = false;
				break;
			case NET_EVENT_PACKET:
				ClientHandlePacket(state, event->packet);
				break;
			default:
				break;
		}
	}

	if (state->connected && !state->helloSent)
	{
		u8 buf[64] = {0};
		WriteStream stream(buf, sizeof(buf));
		HelloPacket packet = {{0,0,0,2}};
		copy_c_str(packet.displayName, state->displayName, sizeof(packet.displayName));

		if (packet.serialize(stream))
		{
			gameMemory->platform.platformClientSend(stream.buffer, stream.pos, 1);
		}
		
		state->helloSent = true;
	}

	ClientSendInput(state, input, gameMemory, mouseWorld);

	Collider2D c = {.position = mouseWorld,
					.size = vec2{0.5f, 0.5f},
					.rotation = 0,
					.type = COLLIDER_CIRCLE};

	vec4 clearColorGreen = {0.5f, 0.714f, 0.486f, 1.0f};
	vec4 clearColor = ColorHexToRBGANormalized(0xC3B67CFF);

	vec2 lineAStart = {0,0};
	vec2 lineAEnd = mouseWorld;
	
	bool lineColliding  = false;
	vec3 lineColor = lineColliding ? vec3{1.0,0,0} : vec3{0,1,0};
	RendererPushSetClear(renderCommands, clearColor);
	RendererPushSetProjection(renderCommands, projection);

    // RendererPushCircle(renderCommands, gdNormal.position, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);

    //RendererPushImage(renderCommands, 2, gdEasy, 4);
    InstanceData2D gdEasy   = {{1.0f, 1.0f, 0.0f}, {0.5f, 0.5f}, 1.0f, 1.0f};
    InstanceData2D gdNormal = {{1.0f, 1.0f, 0.0f}, {10.0f, 10.f}, 1.0f, 1.0f};
    RendererPushImage(renderCommands, 1, gdEasy, 0);
    //RendererPushImage(renderCommands, 1, gdNormal, 0);

   // RendererPushLine(renderCommands, lineAStart, lineAEnd, lineColor, 0.02f, 0);
   	const char * text = "This is some text!";
	vec2 textPos = {0.5,0};
	vec4 textColor = {1.0f, 0.0f, 0.0f, 1.0f};
	TextStyle testTextStyle = {.fillColor = textColor, .strokeWidth = 0.0f};
	f32 fontSize = 0.1f;
	RendererPushText(renderCommands, text, fontSize, state->interFontHandle, textPos, testTextStyle, true, 30);

	SimulateParticles(&state->turretFireEmitter, input->deltaTime);
	SimulateParticles(&state->explosionEmitter,  input->deltaTime);
	SimulateParticles(&state->shellTrailEmitter, input->deltaTime);
	SimulateParticles(&state->impactEmitter,     input->deltaTime);
	DrawParticles(renderCommands, state);


	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		UpdateTankGFX(&state->tanks[i], input->deltaTime);
		DrawTank(state->tanks[i], renderCommands, state);
	}

	mat4 uiProjection = orthographicProjection(input->viewportSize.x, 0.0f, 0.0f, input->viewportSize.y, -1.0f, 100.f);
	RendererPushSetProjection(uiRenderCMDs, uiProjection);

	DrawMainMenu(state, uiRenderCMDs, &gameMemory->platform, input->viewportSize, input->mousePosVP);
}

