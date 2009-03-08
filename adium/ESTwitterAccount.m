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

@implementation ESTwitterAccount

- (const char *)protocolPlugin
{
	return "prpl-mbpurple-twitter";
}

- (void)configurePurpleAccount
{
	[super configurePurpleAccount];
}

- (NSString *)host
{
	return @"twitter.com";
}

@end
