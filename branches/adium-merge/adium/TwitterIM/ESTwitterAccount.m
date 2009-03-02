//
//  ESTwitterAccount.m
//  TwitterIM
//
//  Created by jsippel on 1/7/09.
//

#import <Adium/AIStatusControllerProtocol.h>
#import <Adium/AIAccountControllerProtocol.h>
#import <AIUtilities/AIMenuAdditions.h>
#import <Adium/AISharedAdium.h> 

#import "ESTwitterAccount.h"
//#import "twitter.h"

@implementation ESTwitterAccount

- (const char *)protocolPlugin
{
	return "prpl-twitter";
}

- (void)configurePurpleAccount
{
	[super configurePurpleAccount];
}

@end
