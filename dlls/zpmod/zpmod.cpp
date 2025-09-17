#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "enginecallback.h"
#include "game.h"
#include "edict.h"
#include "zpmod.h"
#include "player.h"
#include "weapons.h"
#include "decals.h"
#include "eiface.h"
#include "gamerules.h"
#include "shake.h"

// #include "zpmod.h" // TODO: eat ass

ZPRound g_round;
TraceResult tr;
int lastSpokeSecond;
ZPPlayer g_players[33];

int RoleToInt(Role r) {
    switch(r) {
        case ROLE_HUMAN: return 1;
        case ROLE_ZOMBIE: return 2;
        case ROLE_SPECTATOR: return 3;
        default: return 0;
    }
}

const char* NumberWord(int n) {
    switch (n) {
        case 1: return "one";
        case 2: return "two";
        case 3: return "three";
        case 4: return "four";
        case 5: return "five";
        case 6: return "six";
        case 7: return "seven";
        case 8: return "eight";
        case 9: return "nine";
        case 10: return "ten";
        default: return "";
    }
}

bool ZPIsZombie(edict_t* player) {
    if (player->v.team == 2) { return true; } else { return false; }
}

bool ZPIsHuman(edict_t* player) {
    if (player->v.team != 2) { return true; } else { return false; }
}

bool ZPIsDead(edict_t* player) {
    if (player->v.team != 1 && player->v.team != 2) { return true; } else { return false; }
}

void ZPPlayerJoin(edict_t* player) {
    int idx = ENTINDEX(player);
    g_players[idx].ed = player;

    if (!player) return;

    char modelName[64] = "player"; // def player model btw

    if (player->v.netname) {
        const char* curModel = STRING(player->v.model);
        if (curModel && curModel[0]) {
            strncpy(modelName, curModel, sizeof(modelName));
        }
    }

    strncpy(g_players[idx].originalModel, modelName, sizeof(g_players[idx].originalModel));

    if (g_round.state == RS_ACTIVE) {
        CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(player);
        if (pPlayer) {
            pPlayer->pev->deadflag = DEAD_DEAD;
            player->v.team = RoleToInt(ROLE_SPECTATOR);
            pPlayer->pev->health = 0;
            pPlayer->StartObserver(pPlayer->pev->origin, pPlayer->pev->angles);
        }
    }
}

void ZPPlayerDisconnect(edict_t* player) {
    int idx = ENTINDEX(player);
    if (idx < 1 || idx > gpGlobals->maxClients) return;

    g_players[idx].ed = nullptr;
    g_players[idx].originalModel[0] = '\0';
    player->v.health = 0;
    // g_players[idx].role = ROLE_HUMAN;
}

void ZPInfectPlayer(edict_t* player, bool wasInfectedBySomeone) {
    CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(player);
    if (!pPlayer) return;
    int idx = ENTINDEX(player);
    if (idx < 1 || idx > gpGlobals->maxClients) return;

    g_players[idx].ZMClass = ZM_CLASS_REGULAR;

    int r = RANDOM_LONG(1, 12);
    
    if (r == 6 && wasInfectedBySomeone == false) {
        g_players[idx].ZMClass = ZM_CLASS_BOSS;
        EMIT_SOUND(player, CHAN_AUTO, "ambience/the_horror3.wav", 1.0, ATTN_NONE);
    } else {
        r = RANDOM_LONG(1, 2);
        if (r == 1) {
            EMIT_SOUND(player, CHAN_AUTO, "zpmod/coming_1.wav", 1.0, 0.2f);
            EMIT_SOUND(player, CHAN_AUTO, "zpmod/human_death_1.wav", 1.0, ATTN_NORM);
        } else {
            EMIT_SOUND(player, CHAN_AUTO, "zpmod/coming_2.wav", 1.0, 0.2f);
            EMIT_SOUND(player, CHAN_AUTO, "zpmod/human_death_2.wav", 1.0, ATTN_NORM);
        }
    }

    // yay effects
    UTIL_ScreenShake(pPlayer->pev->origin, 10.0f, 5.0f, 1.0f, 512.0f);

    Vector red = Vector(255,0,0);
    UTIL_ScreenFade(pPlayer, red, 1.0f, 0.2f, 255, FFADE_IN);

    // zombie stuff
    pPlayer->pev->deadflag = DEAD_NO;
    player->v.health = 500;
    player->v.gravity = 0.8f;

    if (g_players[idx].ZMClass == ZM_CLASS_BOSS) {
        player->v.health = 4000;
        // player->v.gravity = 0.2f;
    }

    hudtextparms_t params;
    memset(&params, 0, sizeof(params));

    char msg[64] = "The boss has awakened...";

    params.channel = 5;
    params.x = -1;
    params.y = 0.1f;
    params.r1 = 255; params.g1 = 0; params.b1 = 0;
    params.a1 = 255;
    params.fadeinTime = 0.3f;
    params.fadeoutTime = 0.3f;
    params.holdTime = 4.0f;
    if (g_players[idx].ZMClass == ZM_CLASS_BOSS) UTIL_HudMessageAll(params, msg);

    pPlayer->RemoveAllItems(false);
    pPlayer->GiveNamedItem("weapon_crowbar");
    pPlayer->SelectItem("weapon_crowbar");

    player->v.team = RoleToInt(ROLE_ZOMBIE);
    player->v.viewmodel = MAKE_STRING("models/zpmod/v_claws.mdl");
    pPlayer->pev->pain_finished = gpGlobals->time;

    ZPSetPlayerModel(player, "zm");

    player->v.modelindex = MODEL_INDEX("models/player/zm/zm.mdl");
    
    // UTIL_Sparks(pPlayer->pev->origin);
    MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pPlayer->pev->origin);
        WRITE_BYTE(TE_BLOODSPRITE);
        WRITE_COORD(pPlayer->pev->origin.x);
        WRITE_COORD(pPlayer->pev->origin.y);
        WRITE_COORD(pPlayer->pev->origin.z + 32);
        WRITE_SHORT(MODEL_INDEX("sprites/blood.spr"));
        WRITE_SHORT(MODEL_INDEX("sprites/blood.spr"));
        WRITE_BYTE(248);
        WRITE_BYTE(20);
    MESSAGE_END();
}

CBaseEntity* ZPZombieCheckHit(CBasePlayer* pPlayer, float range, TraceResult* ptr)
{
    UTIL_MakeVectors(pPlayer->pev->angles);
    Vector vecSrc = pPlayer->pev->origin + pPlayer->pev->view_ofs;
    Vector vecEnd = vecSrc + gpGlobals->v_forward * range;

    // trace that hits monsters players too
    UTIL_TraceLine(vecSrc, vecEnd, dont_ignore_monsters, pPlayer->edict(), ptr);

    if (ptr->flFraction < 1.0f && ptr->pHit && ENTINDEX(ptr->pHit) != 0) {
        return CBaseEntity::Instance(ptr->pHit); // not world
    }
    return nullptr; // world or nothing
}

void ZPDied(edict_t* player) {
    CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(player);
    if (!pPlayer) return;

    if (ZPIsZombie(player)) {
        int r = RANDOM_LONG(1, 2);
        if (r == 1) EMIT_SOUND(player, CHAN_AUTO, "zpmod/death_1.wav", 1.0, ATTN_NORM);
        else if (r == 2) EMIT_SOUND(player, CHAN_AUTO, "zpmod/death_2.wav", 1.0, ATTN_NORM);
    }

    pPlayer->pev->deadflag = DEAD_DEAD;
    player->v.team = RoleToInt(ROLE_SPECTATOR);
    pPlayer->pev->health = 0;
    pPlayer->StartObserver(pPlayer->pev->origin, pPlayer->pev->angles);
}

void ZPHurt(edict_t* player) {
    if (ZPIsZombie(player)) {
        int r = RANDOM_LONG(1, 2);
        if (r == 1) EMIT_SOUND(player, CHAN_AUTO, "zpmod/hurt_1.wav", 1.0, ATTN_NORM);
        else if (r == 2) EMIT_SOUND(player, CHAN_AUTO, "zpmod/hurt_2.wav", 1.0, ATTN_NORM);
    }
}

void ZPZombieSwing(edict_t* player)
{
    CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(player);
    if (!pPlayer) return;
    int idx = ENTINDEX(player);
    if (idx < 1 || idx > gpGlobals->maxClients) return;

    TraceResult tr; 
    CBaseEntity* pHit = ZPZombieCheckHit(pPlayer, 70.0f, &tr);

    if (pHit && pHit->IsPlayer() && ZPIsHuman(pHit->edict())) {
        UTIL_MakeVectors(pPlayer->pev->angles);
        ClearMultiDamage();

        if (g_players[idx].ZMClass == ZM_CLASS_REGULAR) {
            if (pHit->pev->health <= 25.0f)
            {
                pPlayer->pev->health += 100.0f;
                ZPInfectPlayer(pHit->edict(), true);
            }
            else
            {
                pHit->TraceAttack(pPlayer->pev, 25.0f, gpGlobals->v_forward, &tr, DMG_SLASH);
                ApplyMultiDamage(pHit->pev, pPlayer->pev);
            }
        } else {
            pHit->TraceAttack(pPlayer->pev, 35.0f, gpGlobals->v_forward, &tr, DMG_SLASH);
            ApplyMultiDamage(pHit->pev, pPlayer->pev);
        }

        // fuckass sounds
        int r = RANDOM_LONG(1, 3);
        if (r == 1) EMIT_SOUND(player, CHAN_AUTO, "zpmod/attack_1.wav", 1.0, ATTN_NORM);
        else if (r == 2) EMIT_SOUND(player, CHAN_AUTO, "zpmod/attack_2.wav", 1.0, ATTN_NORM);
        else EMIT_SOUND(player, CHAN_AUTO, "zpmod/attack_3.wav", 1.0, ATTN_NORM);
    }
    else if (tr.flFraction < 1.0f && tr.pHit && tr.pHit == ENT(0)) {
        // me when wall :3
        int r = RANDOM_LONG(1, 3);
        if (r == 1) EMIT_SOUND(player, CHAN_AUTO, "zpmod/wall_1.wav", 1.0, ATTN_NORM);
        else if (r == 2) EMIT_SOUND(player, CHAN_AUTO, "zpmod/wall_2.wav", 1.0, ATTN_NORM);
        else EMIT_SOUND(player, CHAN_AUTO, "zpmod/wall_3.wav", 1.0, ATTN_NORM);

        // draw some cool motherfucking decals for example idk fuck this bullshit
        UTIL_DecalTrace(&tr, DECAL_GUNSHOT1);
    }
}

void ZPCountTeams(int& humans, int& zombies)
{
    humans = 0;
    zombies = 0;

    for (int i = 1; i <= gpGlobals->maxClients; i++)
    {
        edict_t* e = INDEXENT(i);
        if (!e || e->free) continue;

        CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(e);
        if (!pPlayer || !pPlayer->IsPlayer() || !pPlayer->IsAlive())
            continue;

        if (ZPIsZombie(e)) zombies++;
        else humans++;
    }
}

void ZPHUD()
{
    int h, z;
    ZPCountTeams(h, z);

    hudtextparms_t params;
    memset(&params, 0, sizeof(params));

    char msg[64];
    sprintf(msg, "%d Humans  |  %d Zombies", h, z);

    params.channel = 3;
    params.x = -1;
    params.y = 0.05f;
    params.r1 = 255; params.g1 = 0; params.b1 = 0;
    params.a1 = 255;
    params.fadeinTime = 0;
    params.fadeoutTime = 0;
    params.holdTime = 1.0f;
    UTIL_HudMessageAll(params, msg);

    for (int i = 1; i <= gpGlobals->maxClients; i++) {
        edict_t* ed = INDEXENT(i);
        if (!ed || ed->free) continue;

        CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(ed);
        if (!pPlayer || !pPlayer->IsPlayer()) continue;

        int hp = (int)pPlayer->pev->health;
        const char* className = "Human";
        if (ZPIsZombie(ed)) {
            className = "Zombie";
            if (g_players[i].ZMClass == ZM_CLASS_BOSS) className = "Boss Zombie";
        }
        else if (ed->v.team == RoleToInt(ROLE_SPECTATOR)) className = "Spectator";

        char buf[64];
        snprintf(buf, sizeof(buf), "Health: %d  Class: %s", hp, className);

        hudtextparms_t info;
        memset(&info, 0, sizeof(info));
        info.channel = 4;
        info.x = 0.02f;
        info.y = 0.90f;

        if (ZPIsZombie(ed)) {
            info.r1 = 255; info.g1 = 0; info.b1 = 0;
        }
        else {
            info.r1 = 0; info.g1 = 255; info.b1 = 0;
        }

        info.a1 = 255;
        info.fadeinTime = 0;
        info.fadeoutTime = 0;
        info.holdTime = 1.0f;

        UTIL_HudMessage(CBaseEntity::Instance(ed), info, buf);
    }
}

void ZPRoundInit(ZPRound* round) {
    round->state = RS_PREP;
    round->nextStateTime = gpGlobals->time + 20.0f; // prep time
    round->lastAnnounce = -1;
    lastSpokeSecond = -1;
    round->countdownStarted = false;
    round->notEnoughPlayersPrinted = false;
}

void ZPRoundStartAmbient() {
    EMIT_AMBIENT_SOUND(
        ENT(0),
        Vector(0,0,0),
        "ambience/wind1.wav",
        1.0,
        ATTN_NONE,
        0,
        100
    );
}

void ZPRoundStopAmbient() {
    EMIT_AMBIENT_SOUND(
        ENT(0),
        Vector(0,0,0),
        "ambience/wind1.wav",
        0,
        ATTN_NONE,
        SND_STOP,
        100
    );
}

void ZPSetPlayerModel(edict_t* player, const char* modelName) {
    if (!player || !modelName) return;

    SET_MODEL(player, modelName);

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "model %s\n", modelName);
    CLIENT_COMMAND(player, cmd);
    //char debug[128];
    //snprintf(debug, sizeof(debug), "say %s\n", modelName);
    //CLIENT_COMMAND(player, debug);
}

void ZPRoundThink(ZPRound* round) {
    CVAR_SET_STRING("sv_skyname", "night");

    int players[32];
    int count = 0;

    ZPHUD();

    int maxClients = gpGlobals->maxClients;
    for (int i = 1; i <= maxClients; i++) {
        edict_t* ed = INDEXENT(i);
        if (ed && !ed->free && ed->v.health > 0) {
            players[count++] = i;
        }
    }

    // if preparing
    if (round->state == RS_PREP) {
        ZPRoundStopAmbient();
        if (count < 2) {
            UTIL_ClientPrintAll(HUD_PRINTCENTER, "Not enough players to start the infection\n");
            round->notEnoughPlayersPrinted = true;
            return;
        }

        if (!round->countdownStarted) {
            round->countdownStarted = true;
            round->nextStateTime = gpGlobals->time + 20.0f;
            lastSpokeSecond = -1;

            for (int i = 1; i <= gpGlobals->maxClients; i++) {
                edict_t* ed = INDEXENT(i);
                if (ed && !ed->free && ed->v.health > 0) {
                    EMIT_SOUND(ed, CHAN_AUTO, "zpmod/start.wav", 1.0, ATTN_NORM);
                }
            }
        }

        float timeLeft = round->nextStateTime - gpGlobals->time;
        int secLeft = (int)ceilf(timeLeft);

        if (secLeft > 0 && secLeft != round->lastAnnounce) {
            round->lastAnnounce = secLeft;

            char msg[64];
            snprintf(msg, sizeof(msg), "Infection in %d", secLeft);
            UTIL_ClientPrintAll(HUD_PRINTCENTER, msg);

            if (secLeft <= 10 && lastSpokeSecond != secLeft) {
                lastSpokeSecond = secLeft;

                const char* word = NumberWord(secLeft);
                if (word[0]) {
                    char path[64];
                    snprintf(path, sizeof(path), "zpmod/vox/%s.wav", word);

                    for (int i = 1; i <= gpGlobals->maxClients; i++) {
                        edict_t* ed = INDEXENT(i);
                        if (!ed || ed->free || ed->v.health <= 0) continue;
                        EMIT_SOUND(ed, CHAN_AUTO, path, 1.0, ATTN_NORM);
                    }
                }
            }
        }

        // time to start infection bc why not lol :3
        if (gpGlobals->time >= round->nextStateTime) {
            round->state = RS_ACTIVE;

            ZPRoundStartAmbient();

            if (count > 0) {
                int idx = RANDOM_LONG(0, count - 1);
                edict_t* chosen = INDEXENT(players[idx]);
                ZPInfectPlayer(chosen, false);
                CLIENT_PRINTF(chosen, print_center, "You are the first zombie\n");
            }
        }
    }

    // if round active
    else if (round->state == RS_ACTIVE) {
        int humans = 0, zombies = 0;
        ZPCountTeams(humans, zombies);

        if (humans == 0 && zombies > 0) {
            round->state = RS_ZOMBIES_WIN;
            round->resetTime = gpGlobals->time + 5.0f;
            UTIL_ClientPrintAll(HUD_PRINTCENTER, "Zombies win\n");
            for (int i = 1; i <= gpGlobals->maxClients; i++) {
                edict_t* ed = INDEXENT(i);
                if (ed && !ed->free && ed->v.health > 0)
                    EMIT_SOUND(ed, CHAN_AUTO, "zpmod/win_zombi.wav", 1.0, ATTN_NORM);
            }
        }
        else if (zombies == 0 && humans > 0) {
            round->state = RS_HUMANS_WIN;
            round->resetTime = gpGlobals->time + 5.0f;
            UTIL_ClientPrintAll(HUD_PRINTCENTER, "Humans win\n");
            for (int i = 1; i <= gpGlobals->maxClients; i++) {
                edict_t* ed = INDEXENT(i);
                if (ed && !ed->free && ed->v.health > 0)
                    EMIT_SOUND(ed, CHAN_AUTO, "zpmod/win_human.wav", 1.0, ATTN_NORM);
            }
        } else if (zombies == 0 && humans == 0) {
            round->state = RS_ROUND_DRAW;
            round->resetTime = gpGlobals->time + 5.0f;
            UTIL_ClientPrintAll(HUD_PRINTCENTER, "Round draw\n");
            for (int i = 1; i <= gpGlobals->maxClients; i++) {
                edict_t* ed = INDEXENT(i);
                if (ed && !ed->free && ed->v.health > 0)
                    EMIT_SOUND(ed, CHAN_AUTO, "zpmod/round_draw.wav", 1.0, ATTN_NORM);
            }
        }
    }

    // if win
    else if (round->state == RS_ZOMBIES_WIN || round->state == RS_HUMANS_WIN || round->state == RS_ROUND_DRAW) {
        if (gpGlobals->time >= round->resetTime) {
            round->state = RS_PREP;
            round->countdownStarted = false;
            round->notEnoughPlayersPrinted = false;
            round->lastAnnounce = -1;
            lastSpokeSecond = -1;

            for (int i = 1; i <= gpGlobals->maxClients; i++) {
                edict_t* ed = INDEXENT(i);
                if (!ed || ed->free) continue;

                CBasePlayer* pPlayer = (CBasePlayer*)GET_PRIVATE(ed);
                if (!pPlayer) continue;

                ed->v.health = 100;

                pPlayer->pev->flags &= ~(FL_SPECTATOR|PFLAG_OBSERVER);
                pPlayer->pev->effects &= ~EF_NODRAW;
                pPlayer->pev->deadflag = DEAD_NO;
                CBaseEntity* spawn = UTIL_FindEntityByClassname(nullptr, "info_player_start");
                if (spawn) {
                    pPlayer->pev->origin = spawn->pev->origin;
                    pPlayer->pev->angles = spawn->pev->angles;
                }
                pPlayer->Spawn();

                int idx = ENTINDEX(ed);
                if (g_players[idx].originalModel[0]) {
                    ZPSetPlayerModel(ed, g_players[idx].originalModel);
                }

                if (ZPIsZombie(pPlayer->edict())) {
                    pPlayer->RemoveAllItems(false);
                    ed->v.team = RoleToInt(ROLE_HUMAN);
                    // pPlayer->GiveNamedItem("weapon_crowbar"); // you already know why i commented this shits out
                    // pPlayer->GiveNamedItem("weapon_glock");
                } else if (ZPIsDead(pPlayer->edict())) {
                    // pPlayer->GiveNamedItem("item_suit");
                }

                pPlayer->pev->iuser1 = 0;
            }
        }
    }
}

void ZPRoundRestart() {
    g_round.state = RS_ROUND_DRAW;
}

// fuck these retards that keeps sending me shit on discord

void ZPModInit(void) {
    ZPRoundInit(&g_round);
    g_engfuncs.pfnAddServerCommand("zpmod_restart", ZPRoundRestart);
    //PRECACHE_MODEL("models/v_claws.mdl");
    //PRECACHE_GENERIC("models/v_claws.mdl");
    // LINK_ENTITY_TO_CLASS(67, CWeaponClaws);
}

void ZPPrecache(void) { // we live in a CRUEL FUCKING WORLD RETARDS..
    PRECACHE_SOUND("zpmod/coming_1.wav");
    PRECACHE_SOUND("zpmod/coming_2.wav");
    PRECACHE_SOUND("zpmod/attack_1.wav");
    PRECACHE_SOUND("zpmod/attack_2.wav");
    PRECACHE_SOUND("zpmod/attack_3.wav");
    PRECACHE_SOUND("zpmod/wall_1.wav");
    PRECACHE_SOUND("zpmod/wall_2.wav");
    PRECACHE_SOUND("zpmod/wall_3.wav");
    PRECACHE_SOUND("zpmod/start.wav");
    PRECACHE_SOUND("zpmod/human_death_1.wav");
    PRECACHE_SOUND("zpmod/human_death_2.wav");
    PRECACHE_SOUND("zombie/zo_attack1.wav");
    PRECACHE_SOUND("zombie/claw_strike1.wav");
    PRECACHE_SOUND("zpmod/win_human.wav");
    PRECACHE_SOUND("zpmod/win_zombi.wav");
    PRECACHE_SOUND("zpmod/round_draw.wav");
    PRECACHE_SOUND("zpmod/vox/ten.wav");
    PRECACHE_SOUND("zpmod/vox/nine.wav");
    PRECACHE_SOUND("zpmod/vox/eight.wav");
    PRECACHE_SOUND("zpmod/vox/seven.wav");
    PRECACHE_SOUND("zpmod/vox/six.wav");
    PRECACHE_SOUND("zpmod/vox/five.wav");
    PRECACHE_SOUND("zpmod/vox/four.wav");
    PRECACHE_SOUND("zpmod/vox/three.wav");
    PRECACHE_SOUND("zpmod/vox/two.wav");
    PRECACHE_SOUND("zpmod/vox/one.wav");
    PRECACHE_SOUND("zpmod/hurt_1.wav");
    PRECACHE_SOUND("zpmod/hurt_2.wav");
    PRECACHE_SOUND("zpmod/death_1.wav");
    PRECACHE_SOUND("zpmod/death_2.wav");
    PRECACHE_SOUND("ambience/the_horror3.wav");

    PRECACHE_MODEL("models/zpmod/v_claws.mdl");
    PRECACHE_GENERIC("models/zpmod/v_claws.mdl");
    PRECACHE_MODEL("models/player/zm/zm.mdl");
    PRECACHE_GENERIC("models/player/zm/zm.mdl");
}