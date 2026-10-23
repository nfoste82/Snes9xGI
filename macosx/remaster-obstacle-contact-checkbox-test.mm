// Native AppKit state regression; the portable helper is the production click
// policy. No game/profile writes, windows or application event loop required.
// c++ -std=c++17 -fobjc-arc -framework Cocoa macosx/remaster-obstacle-contact-checkbox-test.mm -o build/obstacle-contact-checkbox/native-test
#import <Cocoa/Cocoa.h>
#include "../remaster/runtime/obstacle_group_editor.h"
#include <cassert>
#include <iostream>

int main()
{
	@autoreleasepool {
		NSButton *button=[NSButton checkboxWithTitle:@"Support contact" target:nil action:nil];
		button.allowsMixedState=YES;
		RemasterObstacleGroup group;
		group.name="east_end";
		group.members={{{123,1,4},3,0,0,false,false,false},{{123,1,4},3,1,0,false,true,true}};
		std::string error;bool intent=false;
		button.state=NSControlStateValueOff;
		[button setNextState];
		assert(button.state==NSControlStateValueMixed); // Exact cause of old silent no-op.
		assert(S9xRemasterObstacleEditorContactIntent(group,{0},intent,error) && intent);
		assert(S9xRemasterObstacleEditorSetContacts(group,{0},intent,error));
		button.state=group.members[0].supportContact?NSControlStateValueOn:NSControlStateValueOff;
		assert(button.state==NSControlStateValueOn);
		[button setNextState];
		assert(button.state==NSControlStateValueOff);
		assert(S9xRemasterObstacleEditorContactIntent(group,{0},intent,error) && !intent);
		assert(S9xRemasterObstacleEditorSetContacts(group,{0},intent,error));
		button.state=NSControlStateValueMixed;
		[button setNextState];
		assert(S9xRemasterObstacleEditorContactIntent(group,{0,1},intent,error) && intent);
		assert(S9xRemasterObstacleEditorSetContacts(group,{0,1},intent,error));
		assert(S9xRemasterObstacleEditorContactIntent(group,{0,1},intent,error) && !intent);
		assert(!S9xRemasterObstacleEditorSetContacts(group,{0,1},intent,error));
		assert(!error.empty() && group.members[0].supportContact && group.members[1].supportContact);
		std::cout<<"PASS native Off -> Mixed click enables noncontact, On click disables, mixed enables all, last-contact rejection is atomic\n";
	}
}
