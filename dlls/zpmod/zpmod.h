#pragma once

#include "extdll.h"
#include "edict.h"

enum RoundState {
    RS_PREP,
    RS_ACTIVE,
    RS_HUMANS_WIN,
    RS_ZOMBIES_WIN,
    RS_ROUND_DRAW
};

enum Role {
    ROLE_HUMAN,
    ROLE_ZOMBIE,
    ROLE_SPECTATOR
};

enum ZMClasses {
    ZM_CLASS_REGULAR,
    ZM_CLASS_BOSS
};

struct ZPRound {
    int state;
    float resetTime;
    float nextStateTime;
    int lastAnnounce;
    bool countdownStarted;
    bool notEnoughPlayersPrinted;
};

struct ZPPlayer {
    char originalModel[32];
    edict_t* ed;
    ZMClasses ZMClass;
};

extern ZPRound g_round;

void ZPModInit(void);
void ZPRoundThink(ZPRound* round);
void ZPPrecache(void);
void ZPHUD();
bool ZPIsZombie(edict_t* player);
bool ZPIsDead(edict_t* player);
bool ZPIsHuman(edict_t* player);
void ZPZombieSwing(edict_t* player);
void ZPDied(edict_t* player);
void ZPHurt(edict_t* player);
void ZPPlayerJoin(edict_t* player);
void ZPPlayerDisconnect(edict_t* player);
void ZPSetPlayerModel(edict_t* player, const char* modelName);