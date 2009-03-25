//
//  ESTwitterAccount.h
//  TwitterIM
//
//  Created by jsippel on 1/7/09.
//

#import <Cocoa/Cocoa.h>
#import <AdiumLibpurple/CBPurpleAccount.h>
#import "twitter.h"

#define KEY_TWITTER_USE_SSL	@"Twitter:Use SSL"
#define KEY_TWITTER_HIDE_MYSELF @"Twitter:Hide Myself"
#define KEY_TWITTER_REFRESH_RATE @"Twitter:Refresh Rate"

@interface ESTwitterAccount : CBPurpleAccount {

}
@end
