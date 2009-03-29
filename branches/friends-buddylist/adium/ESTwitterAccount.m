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
	
	/* configure SSL */	
	purple_account_set_bool([self purpleAccount], tc_name(TC_USE_HTTPS), [[self preferenceForKey:KEY_TWITTER_USE_SSL
																			 group:GROUP_ACCOUNT_STATUS] boolValue]);
	
	/* configure host */
	NSString	*host = [self preferenceForKey:KEY_CONNECT_HOST group:GROUP_ACCOUNT_STATUS];
	AILog(@"ESTwitterAccount: host=%s", [host UTF8String]);
	purple_account_set_string([self purpleAccount], tc_name(TC_HOST), [[self preferenceForKey:KEY_CONNECT_HOST group:GROUP_ACCOUNT_STATUS] UTF8String]);
	
	/* hide myself */
	purple_account_set_bool([self purpleAccount], tc_name(TC_HIDE_SELF), [[self preferenceForKey:KEY_TWITTER_HIDE_MYSELF
																						   group:GROUP_ACCOUNT_STATUS] boolValue]);
	
	
	/* refresh rate */
	int refreshRate;
	refreshRate = [[self preferenceForKey:KEY_TWITTER_REFRESH_RATE group:GROUP_ACCOUNT_STATUS] intValue];
	AILog(@"ESTwitterAccount: refresh rate=%i", refreshRate);
	purple_account_set_int([self purpleAccount], tc_name(TC_MSG_REFRESH_RATE), refreshRate);
}

@end
