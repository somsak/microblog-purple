//
//  TwitterService.m
//  TwitterIM
//
//  Created by jsippel on 1/7/09.
//

#import "ESTwitterService.h"
#import "ESTwitterAccount.h"

#import <AIUtilities/AIImageAdditions.h>
#import <AIUtilities/AIImageDrawingAdditions.h>

#import <Adium/AIAccountViewController.h>
#import <Adium/AIStatusControllerProtocol.h>
#import <AIUtilities/AIStringUtilities.h>
#import <AIUtilities/AIImageAdditions.h>
#import <Adium/AISharedAdium.h> 

#import "ESTwitterAccountViewController.h"

@implementation ESTwitterService

//Account Creation
- (Class)accountClass{
	return [ESTwitterAccount class];
}

- (AIAccountViewController *)accountViewController{
    return [ESTwitterAccountViewController accountViewController];
}

//Service Description
- (NSString *)serviceCodeUniqueID{
	return @"libpurple-twitter";
}
- (NSString *)serviceID{
	return @"microblog-Twitter";
}
- (NSString *)serviceClass{
	return @"microblog-Twitter";
}
- (NSString *)shortDescription{
	return @"microblog-Twitter";
}
- (NSString *)longDescription{
	return @"microblog-Twitter";
}
- (NSCharacterSet *)allowedCharacters{
	return [NSCharacterSet characterSetWithCharactersInString:@"+abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@._- "];
}
- (NSCharacterSet *)ignoredCharacters{
	return [NSCharacterSet characterSetWithCharactersInString:@""];
}

- (BOOL)caseSensitive{
	return NO;
}
- (BOOL)canCreateGroupChats{
	return NO;
}
- (BOOL)supportsPassword{
	return YES;
}

- (BOOL)requiresPassword
{
	return YES;
}
- (AIServiceImportance)serviceImportance{
	return AIServiceSecondary;
}

/*!
* @brief Placeholder string for the UID field
 */
- (NSString *)UIDPlaceholder
{
	return AILocalizedString(@"username","Sample name for new Twitter accounts");
}

/*!
* @brief Default icon
 *
 * Service Icon packs should always include images for all the built-in Adium services.  This method allows external
 * service plugins to specify an image which will be used when the service icon pack does not specify one.  It will
 * also be useful if new services are added to Adium itself after a significant number of Service Icon packs exist
 * which do not yet have an image for this service.  If the active Service Icon pack provides an image for this service,
 * this method will not be called.
 *
 * The service should _not_ cache this icon internally; multiple calls should return unique NSImage objects.
 *
 * @param iconType The AIServiceIconType of the icon to return. This specifies the desired size of the icon.
 * @return NSImage to use for this service by default
 */


- (NSImage *)defaultServiceIconOfType:(AIServiceIconType)iconType
{
	NSImage *baseImage = [NSImage imageNamed:@"twitter16" forClass:[self class] loadLazily:YES];

	if (iconType == AIServiceIconSmall) {
		baseImage = [baseImage imageByScalingToSize:NSMakeSize(16, 16)];
	}

	return baseImage;
}

- (void)registerStatuses{
	
	AILog(@"ESTwitterService:Setting status");
	[[adium statusController] registerStatus:STATUS_NAME_AVAILABLE
							 withDescription:[[adium statusController] localizedDescriptionForCoreStatusName:STATUS_NAME_AVAILABLE]
									  ofType:AIAvailableStatusType
								  forService:self];
	 }


@end


