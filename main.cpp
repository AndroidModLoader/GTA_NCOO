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

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool (*ProcessLineOfSight)(CVector const&,CVector const&,CColPoint &,CEntity *&,bool,bool,bool,bool,bool,bool,bool,bool);
bool (*GetIsLineOfSightClear)(CVector const&,CVector const&,bool,bool,bool,bool,bool,bool,bool);
void (*SetTask)(CTaskManager*, CTask*, int, bool);
CVector (*FindPlayerCoors)(int);
CWanted* (*FindPlayerWanted)(int);
CTask* (*Task_newOp)(unsigned int bytesSize);
CTask* (*FindTaskByType)(CPedIntelligence*, const int);

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool CanPedClimbNow(CPed* ped)
{
    CVector& startPos = ped->GetPosition();
    CVector& forward = ped->m_matrix->up;
    
    bool bCenterFrontClear = GetIsLineOfSightClear(startPos, startPos + forward * 2.0f, true, false, false, true, false, false, true);
    bool bUpFrontClear = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 0.5f), startPos + CVector(0.0f, 0.0f, 0.5f) + forward * 3.0f, true, false, false, true, false, false, true);
    bool bUpFrontClear2 = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 1.0f), startPos + CVector(0.0f, 0.0f, 1.0f) + forward * 3.0f, true, false, false, true, false, false, true);
    bool bUpFrontClear3 = GetIsLineOfSightClear(startPos + CVector(0.0f, 0.0f, 2.0f), startPos + CVector(0.0f, 0.0f, 2.0f) + forward * 0.5f, true, false, false, true, false, true, true);
    bool bAboveClear = GetIsLineOfSightClear(startPos, startPos + CVector(0.0f, 0.0f, 2.0f), true, false, false, true, false, true, true);

    CColPoint colPoint;
    CEntity* entity;
    bool bDownFrontObstacle = ProcessLineOfSight(startPos - CVector(0.0f, 0.0f, 0.6f), startPos - CVector(0.0f, 0.0f, 0.6f) + forward * 3.0f, colPoint, entity, 
                                                 true, false, false, true, false, true, true, true);

    if (bDownFrontObstacle && bUpFrontClear3)
    {
        float angle = DotProduct(forward, colPoint.m_vecNormal);
        if (abs(angle) < 0.9f || abs(angle) > 1.1f) return false;
    }

    if (bDownFrontObstacle && colPoint.m_vecNormal.z < 0.75f && bCenterFrontClear && bUpFrontClear)
    {
        return true;
    }
    else if ((bCenterFrontClear && bUpFrontClear && bUpFrontClear2) || !bAboveClear)
    {
        return false;
    }
    else
    {
        return bUpFrontClear3 && colPoint.m_vecNormal.z < 0.75f;
    }
}
void ProcessPedClimbIfNeeded(CPed* ped)
{
    if (ped->IsPlayer()) return;
    if (ped->m_bIsStuck || ped->m_PedFlags.bHeadStuckInCollision) return;
    if (!ped->m_pIntelligence) return;
    if (!ped->IsPedInControl() || ped->m_PedFlags.bInVehicle) return;

    if (ped->m_pIntelligence->m_TaskMgr.GetActiveTaskOfType(TASK_COMPLEX_CLIMB) != NULL)
    {
        ped->m_PedFlags.bIsStanding = true;
        return;
    }

    bool bDoesntAttack = true;
    if (ped->m_nPedType == PED_TYPE_COP && FindPlayerWanted(0)->m_nWantedLevel > 0)
    {
        bDoesntAttack = false;
    }
    else
    {
        for (int i = 0; i < TASK_PRIMARY_MAX; ++i)
        {
            if (ped->m_pIntelligence->m_TaskMgr.m_aPrimaryTasks[i])
            {
                eTaskType taskType = ped->m_pIntelligence->m_TaskMgr.m_aPrimaryTasks[i]->GetTaskType();
                if ((taskType >= TASK_COMPLEX_KILL_PED_ON_FOOT && taskType <= TASK_KILL_PED_GROUP_ON_FOOT) || 
                    taskType == TASK_SIMPLE_FIGHT || taskType == TASK_SIMPLE_USE_GUN || taskType == TASK_COMPLEX_BE_IN_GROUP)
                {
                    bDoesntAttack = false;
                    break;
                }
            }
            else if (ped->m_pIntelligence->m_TaskMgr.m_aSecondaryTasks[i])
            {
                eTaskType taskType = ped->m_pIntelligence->m_TaskMgr.m_aSecondaryTasks[i]->GetTaskType();
                if ((taskType >= TASK_COMPLEX_KILL_PED_ON_FOOT && taskType <= TASK_KILL_PED_GROUP_ON_FOOT) || 
                    taskType == TASK_SIMPLE_FIGHT || taskType == TASK_SIMPLE_USE_GUN || taskType == TASK_COMPLEX_BE_IN_GROUP)
                {

                    bDoesntAttack = false;
                    break;
                }
            }
        }
    }
    if (bDoesntAttack) return;

    CVector pedCoors = FindPlayerCoors(0);
    if (DistanceBetweenPoints(pedCoors, ped->GetPosition()) < 2.0f) return;

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

    if (ped->m_vecMoveSpeed.Magnitude() < 0.01f)
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
extern "C" void OnModPreLoad()
{
    logger->SetTag("NCOO");
    
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    // GTA Variables
    SET_TO(_ZTV17CTaskComplexClimb,         aml->GetSym(hGTASA, "_ZTV17CTaskComplexClimb")); _ZTV17CTaskComplexClimb += 2 * sizeof(void*);

    // GTA Functions
    SET_TO(ProcessLineOfSight,              aml->GetSym(hGTASA, "_ZN6CWorld18ProcessLineOfSightERK7CVectorS2_R9CColPointRP7CEntitybbbbbbbb"));
    SET_TO(GetIsLineOfSightClear,           aml->GetSym(hGTASA, "_ZN6CWorld21GetIsLineOfSightClearERK7CVectorS2_bbbbbbb"));
    SET_TO(SetTask,                         aml->GetSym(hGTASA, "_ZN12CTaskManager7SetTaskEP5CTaskib"));
    SET_TO(FindPlayerCoors,                 aml->GetSym(hGTASA, "_Z15FindPlayerCoorsi"));
    SET_TO(FindPlayerWanted,                aml->GetSym(hGTASA, "_Z16FindPlayerWantedi"));
    SET_TO(Task_newOp,                      aml->GetSym(hGTASA, BYBIT("_ZN5CTasknwEj", "_ZN5CTasknwEm")));
    SET_TO(FindTaskByType,                  aml->GetSym(hGTASA, "_ZNK16CPedIntelligence14FindTaskByTypeEi"));

    // GTA Hooks
    HOOK(ProcessPedControl,                 aml->GetSym(hGTASA, "_ZN4CPed14ProcessControlEv"));
}
