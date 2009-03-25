//
//  ESTwitterAccountViewController.m
//  TwitterIM
//
//  Created by jsippel on 3/9/09.
//

#import "ESTwitterAccountViewController.h"
#import "ESTwitterAccount.h"

//@implementation NSObject
@implementation ESTwitterAccountViewController

/*!
 * @brief Nib name
 */
- (NSString *)nibName{
    return @"ESTwitterAccountView";
}

- (void)configureForAccount:(AIAccount *)inAccount
{
    [super configureForAccount:inAccount];
	
	//Connection security
	[checkbox_useSSL setState:[[account preferenceForKey:KEY_TWITTER_USE_SSL group:GROUP_ACCOUNT_STATUS] boolValue]];
	
	//Hide myself in conversation
	[checkbox_hideMyself setState:[[account preferenceForKey:KEY_TWITTER_HIDE_MYSELF group:GROUP_ACCOUNT_STATUS] boolValue]];

	//Refresh Rate
	NSNumber	*refreshRate = [account preferenceForKey:KEY_TWITTER_REFRESH_RATE group:GROUP_ACCOUNT_STATUS];
	if (refreshRate) {
		[textField_refreshRate setIntValue:[refreshRate intValue]];
	} else {
		
		[textField_refreshRate setStringValue:@"60"];
	}
}

- (void)saveConfiguration
{
	[super saveConfiguration];
	
	[account setPreference:[NSNumber numberWithBool:[checkbox_useSSL state]]
					forKey:KEY_TWITTER_USE_SSL group:GROUP_ACCOUNT_STATUS];
	
	[account setPreference:[NSNumber numberWithBool:[checkbox_hideMyself state]]
					forKey:KEY_TWITTER_HIDE_MYSELF group:GROUP_ACCOUNT_STATUS];
	
	//Refresh Rate
	[account setPreference:([textField_refreshRate intValue] ? [NSNumber numberWithInt:[textField_refreshRate intValue]] : nil)
					forKey:KEY_TWITTER_REFRESH_RATE
					 group:GROUP_ACCOUNT_STATUS];
	
	
}	

@end
