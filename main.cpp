#include <mod/amlmod.h>
#include <mod/logger.h>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
#else
    #include "GTASA_STRUCTS_210.h"
#endif

MYMOD(net.cowboy69.rusjj.ncoo, GTA NPC Climb Over Obstacles, 1.2, Cowboy69 & RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0.2.1)
END_DEPLIST()

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t _ZTV17CTaskComplexClimb;
CEntity* g_pLastTarget;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool (*ProcessLineOfSight)(CVector const&,CVector const&,CColPoint &,CEntity *&,bool,bool,bool,bool,bool,bool,bool,bool);
bool (*GetIsLineOfSightClear)(CVector const&,CVector const&,bool,bool,bool,bool,bool,bool,bool);
void (*SetTask)(CTaskManager*, CTask*, int, bool);
CWanted* (*FindPlayerWanted)(int);
CTask* (*Task_newOp)(uintptr_t bytesSize);
CTask* (*FindTaskByType)(CPedIntelligence*, const int);
CPed* (*FindPlayerPed)(int);

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
inline bool CanPedClimbNow(CPed* ped)
{
    CVector& startPos = ped->GetPosition();
    CVector& forward = ped->m_matrix->up;
    
    bool bUpFrontClear3 = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 2.0f), startPos + CVector(0.0f, 0.0f, 2.0f) + forward * 0.5f, true, false, false, true, false, true, true);
    
    CColPoint colPoint;
    CEntity* entity;
    bool bDownFrontObstacle = ProcessLineOfSight(startPos - CVector(0.0f, 0.0f, 0.6f), startPos - CVector(0.0f, 0.0f, 0.6f) + forward * 3.0f, colPoint, entity, 
                                                 true, false, false, true, false, true, true, true);

    if (bDownFrontObstacle && bUpFrontClear3)
    {
        float angle = DotProduct(forward, colPoint.m_vecNormal);
        if (fabsf(angle) < 0.9f || fabsf(angle) > 1.1f) return false;
    }

    bool bCenterFrontClear = GetIsLineOfSightClear(startPos, startPos + forward * 2.0f, true, false, false, true, false, false, true);
    bool bUpFrontClear = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 0.5f), startPos + CVector(0.0f, 0.0f, 0.5f) + forward * 3.0f, true, false, false, true, false, false, true);
    if (bDownFrontObstacle && colPoint.m_vecNormal.z < 0.75f && bCenterFrontClear && bUpFrontClear)
    {
        return true;
    }
    
    bool bUpFrontClear2 = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 1.0f), startPos + CVector(0.0f, 0.0f, 1.0f) + forward * 3.0f, true, false, false, true, false, false, true);
    bool bAboveClear = GetIsLineOfSightClear(startPos, startPos + CVector(0.0f, 0.0f, 2.0f), true, false, false, true, false, true, true);
    if ((bCenterFrontClear && bUpFrontClear && bUpFrontClear2) || !bAboveClear)
    {
        return false;
    }
    
    return (bUpFrontClear3 && colPoint.m_vecNormal.z < 0.75f);
}
inline bool DoesTaskMeetRequirements(CPed* ped, CTask* task)
{
    switch(task->GetTaskType())
    {
        default: return false;

        case TASK_COMPLEX_KILL_PED_ON_FOOT:
        {
            CTaskComplexKillPedOnFoot* tt = (CTaskComplexKillPedOnFoot*)task;
            if(tt->m_pTargetPed != NULL)
            {
                g_pLastTarget = tt->m_pTargetPed;
                break;
            }
            return false;
        }

        case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE:
        {
            CTaskComplexKillPedOnFootMelee* tt = (CTaskComplexKillPedOnFootMelee*)task;
            if(tt->m_pTargetPed != NULL)
            {
                g_pLastTarget = tt->m_pTargetPed;
                break;
            }
            return false;
        }

        case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED:
        {
            CTaskComplexKillPedOnFootArmed* tt = (CTaskComplexKillPedOnFootArmed*)task;
            if(tt->m_pTargetPed != NULL)
            {
                g_pLastTarget = tt->m_pTargetPed;
                break;
            }
            return false;
        }

        case TASK_COMPLEX_DESTROY_CAR:
        case TASK_COMPLEX_DESTROY_CAR_MELEE:
        case TASK_COMPLEX_DESTROY_CAR_ARMED:
        {
            CTaskComplexDestroyCar* tt = (CTaskComplexDestroyCar*)task;
            if(tt->m_pTargetVehicle != NULL)
            {
                g_pLastTarget = tt->m_pTargetVehicle;
                break;
            }
            return false;
        }

        case TASK_KILL_PED_GROUP_ON_FOOT:
        {
            CTaskComplexKillPedGroupOnFoot* tt = (CTaskComplexKillPedGroupOnFoot*)task;
            if(tt->m_pCurrentPed != NULL)
            {
                g_pLastTarget = tt->m_pCurrentPed;
                break;
            }
            return false;
        }

        case TASK_SIMPLE_FIGHT:
        {
            CTaskSimpleFight* tt = (CTaskSimpleFight*)task;
            if(tt->m_pTargetEntity != NULL)
            {
                g_pLastTarget = tt->m_pTargetEntity;
                break;
            }
            return false;
        }

        case TASK_SIMPLE_USE_GUN:
        {
            CTaskSimpleUseGun* tt = (CTaskSimpleUseGun*)task;
            if(tt->m_pTargetEntity != NULL)
            {
                g_pLastTarget = tt->m_pTargetEntity;
                break;
            }
            return false;
        }
    }
    return true;
}
inline void ProcessPedClimbIfNeeded(CPed* ped)
{
    if (!ped->m_pIntelligence || ped->IsPlayer() || !ped->IsPedInControl()) return;
    if (ped->m_bIsStuck || ped->m_PedFlags.bHeadStuckInCollision || ped->m_PedFlags.bInVehicle) return;

    if (ped->m_pIntelligence->m_TaskMgr.GetActiveTaskOfType(TASK_COMPLEX_CLIMB) != NULL)
    {
        ped->m_PedFlags.bIsStanding = true;
        return;
    }

    g_pLastTarget = FindPlayerPed(-1);
    if (ped->m_nPedType == PED_TYPE_COP && FindPlayerWanted(0)->m_nWantedLevel > 0)
    {
        // Cops should definitely follow us.
    }
    else
    {
        for (int i = 0; i < TASK_PRIMARY_MAX; ++i)
        {
            if (ped->m_pIntelligence->m_TaskMgr.m_aPrimaryTasks[i])
            {
                if(DoesTaskMeetRequirements(ped, ped->m_pIntelligence->m_TaskMgr.m_aPrimaryTasks[i]))
                {
                    break;
                }
            }
            else if (ped->m_pIntelligence->m_TaskMgr.m_aSecondaryTasks[i])
            {
                if(DoesTaskMeetRequirements(ped, ped->m_pIntelligence->m_TaskMgr.m_aSecondaryTasks[i]))
                {
                    break;
                }
            }
            if(i == TASK_PRIMARY_MAX-1) return;
        }
    }

    if(g_pLastTarget)
    {
        CVector& pos = ped->GetPosition();
        CVector& tpos = g_pLastTarget->GetPosition();
        if (pos.z > tpos.z || DistanceBetweenPoints(pos, tpos) < 2.0f) return;
    }
    if (!CanPedClimbNow(ped)) return;

    CTaskComplexClimb* climbTask = (CTaskComplexClimb*)Task_newOp(sizeof(CTaskComplexClimb));
    // Initialisation sentence
    climbTask->vtable() = _ZTV17CTaskComplexClimb;
    climbTask->m_pParentTask = NULL;
    climbTask->m_pSubTask = NULL;
    climbTask->m_nForceClimb = 1;
    climbTask->m_bUsePlayerLaunchForce = false;
    // Initialisation sentence end
    SetTask(&ped->m_pIntelligence->m_TaskMgr, climbTask, TASK_PRIMARY_EVENT_RESPONSE_TEMP, false);

    if (ped->m_vecMoveSpeed.MagnitudeSqr() < 0.0001f)
    {
        ped->m_vecMoveSpeed += CVector(0.0f, 0.0f, 0.08f) + ped->m_matrix->up * 0.1f;
    }
    ped->m_PedFlags.bIsStanding = false;
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
DECL_HOOKv(ProcessPedControl, CPed* ped)
{
    ProcessPedClimbIfNeeded(ped);
    ProcessPedControl(ped);
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Main     ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
ON_MOD_PRELOAD()
{
    logger->SetTag("NCOO");
    
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    // GTA Variables
    SET_TO(_ZTV17CTaskComplexClimb, aml->GetSym(hGTASA, "_ZTV17CTaskComplexClimb")); _ZTV17CTaskComplexClimb += 2 * sizeof(void*);

    // GTA Functions
    SET_TO(ProcessLineOfSight,      aml->GetSym(hGTASA, "_ZN6CWorld18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntitybbbbbbbb"));
    SET_TO(GetIsLineOfSightClear,   aml->GetSym(hGTASA, "_ZN6CWorld21GetIsLineOfSightClearERK7CVectorS2_bbbbbbb"));
    SET_TO(SetTask,                 aml->GetSym(hGTASA, "_ZN12CTaskManager7SetTaskEP5CTaskib"));
    SET_TO(FindPlayerWanted,        aml->GetSym(hGTASA, "_Z16FindPlayerWantedi"));
    SET_TO(Task_newOp,              aml->GetSym(hGTASA, BYBIT("_ZN5CTasknwEj", "_ZN5CTasknwEm")));
    SET_TO(FindTaskByType,          aml->GetSym(hGTASA, "_ZNK16CPedIntelligence14FindTaskByTypeEi"));
    SET_TO(FindPlayerPed,           aml->GetSym(hGTASA, "_Z13FindPlayerPedi"));

    // GTA Hooks
    HOOK(ProcessPedControl,         aml->GetSym(hGTASA, "_ZN4CPed14ProcessControlEv"));
}
