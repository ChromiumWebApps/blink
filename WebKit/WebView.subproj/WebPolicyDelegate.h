/*	
        WebPolicyDelegate.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebView;
@class WebError;
@class WebFrame;
@class WebPolicyPrivate;
@class WebResponse;
@class NSURLRequest;


/*!
  @enum WebNavigationType
  @abstract The type of action that triggered a possible navigation.
  @constant WebNavigationTypeLinkClicked A link with an href was clicked.
  @constant WebNavigationTypeFormSubmitted A form was submitted.
  @constant WebNavigationTypeBackForward The user chose back or forward.
  @constant WebNavigationTypeReload The User hit the reload button.
  @constant WebNavigationTypeFormResubmitted A form was resubmitted (by virtue of doing back, forward or reload).
  @constant WebNavigationTypeOther Navigation is taking place for some other reason.
*/

typedef enum {
    WebNavigationTypeLinkClicked,
    WebNavigationTypeFormSubmitted,
    WebNavigationTypeBackForward,
    WebNavigationTypeReload,
    WebNavigationTypeFormResubmitted,
    WebNavigationTypeOther
} WebNavigationType;


extern NSString *WebActionNavigationTypeKey; // NSNumber (WebActionType)
extern NSString *WebActionElementKey; // NSDictionary of element info
extern NSString *WebActionButtonKey;  // NSEventType
extern NSString *WebActionModifierFlagsKey; // NSNumber (unsigned)
extern NSString *WebActionOriginalURLKey; // NSURL


/*!
    @protocol WebPolicyDecisionListener
    @discussion This protocol is used to call back with the results of a
    policy decision. This provides the ability to make these decisions
    asyncrhonously, which means the decision can be made by prompting
    with a sheet, for example.
*/

@protocol WebPolicyDecisionListener <NSObject>

/*!
    @method use
    @abstract Use the resource
    @discussion If there remain more policy decisions to be made, then
    the next policy delegate method gets to decide. This will be
    either the next navigation policy delegate if there is a redirect,
    or the content policy delegate. If there are no more policy
    decisions to be made, the resource will be displayed inline if
    possible. If there is no view available to display the resource
    inline, then unableToImplementPolicyWithError:inFrame: will be
    called with an appropriate error. 

    <p>If a new window is going to be created for this navigation as a
    result of frame targetting, then it will be created once you call
    this method.
*/
-(void)use;
/*!
    @method download
    @abstract Download the resource instead of displaying it.
    @discussion This method is more than just a convenience because it
    allows an in-progress navigation to be converted to a download
    based on content type, without having to stop and restart the
    load.
*/
-(void)download;

/*!
    @method ignore
    @abstract Do nothing (but the client may choose to handle the request itself)
    @discussion A policy of ignore prevents WebKit from doing anything
    further with the load, however, the client is still free to handle
    the request in some other way, such as opening a new window,
    opening a new window behind the current one, opening the URL in an
    external app, revealing the location in Finder if a file URL, etc.
*/
-(void)ignore;

@end


/*!
    @category WebPolicyDelegate
    @discussion While loading a URL, WebKit asks the WebControllerPolicyDelegate for
    policies that determine the action of what to do with the URL or the data that
    the URL represents. Typically, the policy handler methods are called in this order:

    decideNewWindowPolicyForAction:andRequest:newFrameName:decisionListener: (at most once)<BR>
    decideNavigationPolicyForAction:inFrame::decisionListener: (zero or more times)<BR>
    decideContentPolicyForMIMEType:andRequest:inFrame: (zero or more times)<BR>

    New window policy is always checked. Navigation policy is checked
    for the initial load and every redirect unless blocked by an
    earlier policy. Content policy is checked once the content type is
    known, unless an earlier policy prevented it.

    In rare cases, content policy might be checked more than
    once. This occurs when loading a "multipart/x-mixed-replace"
    document, also known as "server push". In this case, multiple
    documents come in one navigation, with each replacing the last. In
    this case, conent policy will be checked for each one.
*/
@interface NSObject (WebPolicyDelegate)

/*!
   @method decideNavigationPolicyForAction:andRequest:inFrame:decisionListener:
   @abstract This method is called to decide what to do with a proposed navigation.
   @param actionInformation Dictionary that describes the action that triggered this navigation.
   @param request The request for the proposed navigation
   @param frame The WebFrame in which the navigation is happening
   @param listener The object to call when the decision is made
   @discussion This method will be called before loading starts, and
   on every redirect.
*/
- (void)webView:(WebView *)webView decideNavigationPolicyForAction:(NSDictionary *)actionInformation
                             andRequest:(NSURLRequest *)request
                                inFrame:(WebFrame *)frame
                       decisionListener:(id<WebPolicyDecisionListener>)listener;

/*!
     @method decideNewWindowPolicyForAction:andRequest:newFrameName:decisionListener:
     @discussion This method is called to decide what to do with an targetted nagivation that would open a new window.
     @param actionInformation Dictionary that describes the action that triggered this navigation.
     @param request The request for the proposed navigation
     @param frame The frame in which the navigation is taking place
     @param listener The object to call when the decision is made
     @discussion This method is provided so that modified clicks on a targetted link which
     opens a new frame can prevent the new window from being opened if they decide to
     do something else, like download or present the new frame in a specialized way. 

     <p>If this method picks a policy of Use, the new window will be
     opened, and decideNavigationPolicyForAction:andRequest:inFrame:decisionListner:
     will be called with a WebNavigationType of WebNavigationTypeOther
     in its action. This is to avoid possible confusion about the modifiers.
*/
- (void)webView:(WebView *)webView decideNewWindowPolicyForAction:(NSDictionary *)actionInformation
                            andRequest:(NSURLRequest *)request
                          newFrameName:(NSString *)frameName
                      decisionListener:(id<WebPolicyDecisionListener>)listener;

/*!
    @method decideContentPolicyForMIMEType:andRequest:inFrame:
    @discussion Returns the policy for content which has been partially loaded. Sent after locationChangeStarted. 
    @param type MIME type for the resource.
    @param request A WebResourceRequest for the partially loaded content.
    @param frame The frame which is loading the URL.
    @param listener The object to call when the decision is made
*/
- (void)webView:(WebView *)webView decideContentPolicyForMIMEType:(NSString *)type
                                 andRequest:(NSURLRequest *)request
                                    inFrame:(WebFrame *)frame
                           decisionListener:(id<WebPolicyDecisionListener>)listener;

/*!
    @method unableToImplementPolicy:error:forURL:inFrame:
    @discussion Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
    @param policy The policy that could not be implemented.
    @param error The error that caused the policy to not be implemented.
    @param URL The URL of the resource for which a particular action was requested but failed.
    @param frame The frame in which the policy could not be implemented.
*/
- (void)webView:(WebView *)webView unableToImplementPolicyWithError:(WebError *)error inFrame:(WebFrame *)frame;

@end
