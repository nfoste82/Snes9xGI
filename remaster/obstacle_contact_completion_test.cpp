#include "obstacle_group_editor.h"
#include <cassert>
#include <iostream>

static RemasterFrame mirror(const RemasterFrame &input, int flip)
{
	auto result=input;
	for (auto &instance : result.tileInstances) {
		instance.hFlip=instance.hFlip!=bool(flip&1);
		instance.vFlip=instance.vFlip!=bool(flip&2);
	}
	for (uint32_t y=0;y<input.height;y++) for (uint32_t x=0;x<input.width;x++) {
		const size_t src=size_t(y)*input.width+x;
		const size_t dst=size_t((flip&2)?input.height-1-y:y)*input.width+((flip&1)?input.width-1-x:x);
		result.mainPixels[dst]=input.mainPixels[src]; result.subPixels[dst]=input.subPixels[src];
	}
	return result;
}

static std::string text(const RemasterProfile &profile)
{
	std::string result; std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterSerializeProfile(profile,result,diagnostics)); return result;
}

int main(int argc, char **argv)
{
	assert(argc>=3);
	RemasterProfile profile; std::vector<RemasterProfileDiagnostic> diagnostics;
	assert(S9xRemasterLoadProfile(argv[1],profile,diagnostics));
	const auto original=text(profile);
	for (int fixture=2;fixture<argc;fixture++) {
		RemasterFrame frame; assert(S9xReadRemasterFrame(argv[fixture],frame));
		for (const auto &group : profile.obstacleGroups) if (group.spriteAssembly) {
			std::set<RemasterTileContentId> identities;
			for (const auto &m : group.members) identities.insert(m.tileId);
			std::map<std::pair<int,int>,RemasterObstacleEditorCell> occurrences;
			for (uint32_t y=0;y<frame.height;y++) for (uint32_t x=0;x<frame.width;x++) {
				RemasterObstacleEditorCell c;
				if (S9xRemasterObstacleEditorCellAt(frame,x,y,c) && c.spriteAssembly && identities.count(c.tileId))
					occurrences[{c.x,c.y}]=c;
			}
			assert(group.members.size()==16 && occurrences.size()==24);
			int bottom=-10000; for (const auto &entry : occurrences) bottom=std::max(bottom,entry.second.y);
			std::vector<RemasterObstacleEditorCell> row;
			for (const auto &entry : occurrences) if (entry.second.y==bottom) row.push_back(entry.second);
			assert(row.size()==6);
			std::vector<size_t> members; std::string error;
			assert(!S9xRemasterObstacleEditorResolveSelection(frame,group,row,members,error));
			assert(error=="Selected occurrences do not match one authored obstacle footprint.");
			for (int flip=0;flip<4;flip++) {
				auto mirrored=mirror(frame,flip); auto selected=row;
				for (auto &c : selected) {
					if (flip&1) c.x=int(frame.width)-8-c.x;
					if (flip&2) c.y=int(frame.height)-8-c.y;
					c.hflip=c.hflip!=bool(flip&1); c.vflip=c.vflip!=bool(flip&2);
				}
				// Custom nonboundary contact survives; distinct authored Z values
				// are preserved. Repeated identities retain consistent Z so new
				// repeats can copy it without guessing which authored Z applies.
				auto authored=group;
				for (auto &m : authored.members) {
					m.z=int(m.tileId.hash%11)-5;
					m.supportContact=(m.y==0 && m.x==1);
				}
				assert(std::any_of(authored.members.begin(),authored.members.end(),[](const auto &m){return m.supportContact;}));
				RemasterObstacleGroup proposal; bool complete=false;
				assert(S9xRemasterObstacleEditorPrepareContactSelection(mirrored,authored,selected,proposal,members,complete,error));
				assert(complete && proposal.members.size()==24 && members.size()==6);
				assert(S9xRemasterObstacleEditorSetContacts(proposal,members,true,error));
				for (size_t i=0;i<authored.members.size();i++) {
					const auto &a=authored.members[i], &b=proposal.members[i];
					assert(a.tileId==b.tileId && a.x==b.x && a.y==b.y && a.z==b.z && a.hflip==b.hflip && a.vflip==b.vflip);
					assert(b.supportContact==(a.supportContact || std::find(members.begin(),members.end(),i)!=members.end()));
				}
				for (size_t i=authored.members.size();i<proposal.members.size();i++)
					assert(proposal.members[i].supportContact==(std::find(members.begin(),members.end(),i)!=members.end()));
				// Cancel is simply dropping the proposal. One serialized profile
				// snapshot restores the entire addition+contact transaction.
				auto saved=profile; saved.obstacleGroups={proposal}; const auto after=text(saved);
				RemasterProfile decoded; assert(S9xRemasterParseProfile(after,decoded,diagnostics)); assert(text(decoded)==after);
				assert(text(profile)==original);
				// Current saved row is mixed once missing cells are provisionally
				// added: first click enables six, second disables six while the
				// existing interior (row 2, col 3) contact remains untouched.
				assert(S9xRemasterObstacleEditorPrepareContactSelection(mirrored,group,selected,proposal,members,complete,error));
				bool anyOn=false, anyOff=false;
				for (auto i : members) (proposal.members[i].supportContact?anyOn:anyOff)=true;
				assert(anyOn && anyOff);
				assert(S9xRemasterObstacleEditorSetContacts(proposal,members,true,error));
				for (auto i : members) assert(proposal.members[i].supportContact);
				assert(S9xRemasterObstacleEditorSetContacts(proposal,members,false,error));
				for (auto i : members) assert(!proposal.members[i].supportContact);
				assert(std::any_of(proposal.members.begin(),proposal.members.end(),[](const auto &m){return m.x==3 && m.y==2 && m.supportContact;}));
				// Failure after completion must not mutate the actual input.
				auto onlyBottom=group;
				for (auto &m : onlyBottom.members) m.supportContact=m.y==3;
				assert(S9xRemasterObstacleEditorPrepareContactSelection(mirrored,onlyBottom,selected,proposal,members,complete,error));
				assert(!S9xRemasterObstacleEditorSetContacts(proposal,members,false,error));
				assert(!error.empty() && onlyBottom.members.size()==16);
				// No completion escape hatch for background or foreign sources.
				auto bg=group; bg.spriteAssembly=false;
				assert(!S9xRemasterObstacleEditorPrepareContactSelection(mirrored,bg,selected,proposal,members,complete,error));
				auto foreign=selected; foreign.back().background=1;
				assert(!S9xRemasterObstacleEditorPrepareContactSelection(mirrored,group,foreign,proposal,members,complete,error));
				foreign=selected; foreign.back().tileId.hash++;
				assert(!S9xRemasterObstacleEditorPrepareContactSelection(mirrored,group,foreign,proposal,members,complete,error));
			}
			// Two complete captured motifs make completion itself ambiguous.
			// Selection must not be used as permission to guess ownership.
			auto doubled=frame;
			for (uint32_t y=0;y<frame.height;y++) for (uint32_t x=0;x<frame.width;x++) {
				RemasterObstacleEditorCell c;
				if (!S9xRemasterObstacleEditorCellAt(frame,x,y,c) || !c.spriteAssembly || !identities.count(c.tileId)) continue;
				const int dx=int(x)-96;
				assert(dx>=0);
				doubled.mainPixels[size_t(y)*frame.width+dx]=frame.mainPixels[size_t(y)*frame.width+x];
				doubled.subPixels[size_t(y)*frame.width+dx]=frame.subPixels[size_t(y)*frame.width+x];
			}
			RemasterObstacleGroup ambiguous; bool complete=false;
			assert(!S9xRemasterObstacleEditorPrepareContactSelection(doubled,group,row,ambiguous,members,complete,error));
			std::cout<<"PASS "<<argv[fixture]<<": reproduced bottom-six failure; 16→24 proposal, all mirrors, custom contacts/Z preservation, atomic failure, read-only roundtrip\n";
		}
	}
	assert(text(profile)==original);
}
