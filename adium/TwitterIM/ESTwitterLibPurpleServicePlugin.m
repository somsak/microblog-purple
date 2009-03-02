//
//  ESTwitterLibPurpleServicePlugin.m
//  TwitterIM
//
//  Created by jsippel on 1/7/09.
//

#import <Adium/AIPlugin.h>
#import <AdiumLibpurple/AILibpurplePlugin.h>

#import "ESTwitterLibPurpleServicePlugin.h"
#import "ESTwitterService.h"
	

@implementation ESTwitterLibPurpleServicePlugin

//extern BOOL purple_init_plugin(void);


- (void) installPlugin
{
	[ESTwitterService registerService];
	AILog(@"Installing Twitter Plugin");
	//purple_init_twitterim_plugin();
}

- (void)loadLibpurplePlugin
{
	AILog(@"Loading Twitter Plugin");
}

- (void) installLibpurplePlugin
{
	AILog(@"Loading Twitter LibPurple Plugin");
	purple_init_twitterim_plugin();
}

@end
