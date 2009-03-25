//
//  ESTwitterAccountViewController.h
//  TwitterIM
//
//  Created by jsippel on 3/9/09.
//

#import <Cocoa/Cocoa.h>
#import <AdiumLibpurple/PurpleAccountViewController.h>

//@interface NSObject : /* Specify a superclass (eg: NSObject or NSView) */ {

@interface ESTwitterAccountViewController : PurpleAccountViewController {
	IBOutlet	NSButton	*checkbox_useSSL;
	IBOutlet	NSButton	*checkbox_hideMyself;
	IBOutlet	NSTextField *textField_refreshRate;
	
}

@end