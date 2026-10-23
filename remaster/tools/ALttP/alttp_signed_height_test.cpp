// Standalone local-profile regression; no room geometry is authored here.
#include "../../runtime/diagonal_assembly.h"
#include <cstdlib>
#include <iostream>
#include <type_traits>

static void need(bool value) { if(!value)std::abort(); }
int main()
{
 static_assert(std::is_same<decltype(RemasterDiagonalAssemblyGeometry::height),std::array<int16_t,64>>::value,
               "authored geometry must retain signed int16 samples");
 std::vector<RemasterDiagonalAssemblyMember> members={{17,-300,1,true,true,true,false}};
 std::vector<RemasterDiagonalAssemblyGeometry> geometry;
 RemasterDiagonalAssemblyContacts contacts={-320,-280,true,true,true,false};
 need(S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 need(geometry[0].height[0]==-300 && geometry[0].height[63]==-314);
 contacts.minimum=-32768;contacts.maximum=32767;members[0].intercept=32767;
 need(S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 need(geometry[0].height[0]==32767 && geometry[0].height[63]==32753);
 members[0].intercept=-32768;
 need(S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 need(geometry[0].height[63]==-32768);
 contacts.minimum=-32769;need(!S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 contacts.minimum=0;contacts.maximum=32768;need(!S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 contacts.maximum=512;members[0].intercept=512;
 need(S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 need(geometry[0].height[0]==512);
 members.push_back(members[0]);members.back().writer=2;
 need(!S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 members.pop_back();members[0].ownershipVerified=false;
 need(!S9xRemasterSolveDiagonalAssembly(contacts,members,geometry));
 std::cout<<"signed diagonal local-height regression passed\n";
}
