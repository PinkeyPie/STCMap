#ifndef CS_H
#define CS_H

struct CsInput 
{
    uint3 GroupId           : SV_GroupID;           // 3D index of the thread group in the dispatch
    uint3 GroupThreadId     : SV_GroupThreadID;     // 3D index of the thread within the thread group
    uint3 DispatchThreadId  : SV_DispatchThreadID;  // 3D index of the thread within the dispatch
    uint  GroupIndex        : SV_GroupIndex;        // 1D index of the thread within the thread group
};

#endif