/*  HBPresetsViewController.m $

 This file is part of the HandBrake source code.
 Homepage: <http://handbrake.fr/>.
 It may be used under the terms of the GNU General Public License. */

#import "HBPresetsViewController.h"
#import "HBPresetsManager.h"
#import "HBPreset.h"

// drag and drop pasteboard type
#define kHandBrakePresetPBoardType @"handBrakePresetPBoardType"

@interface HBPresetsViewController () <NSOutlineViewDelegate>

@property (nonatomic, retain) HBPresetsManager *presets;
@property (nonatomic, assign) IBOutlet NSTreeController *treeController;

/**
 *  Helper var for drag & drop
 */
@property (nonatomic, retain) NSArray *dragNodesArray;

/**
 *  The status (expanded or not) of the folders.
 */
@property (nonatomic, retain) NSMutableArray *expandedNodes;

@property (assign) IBOutlet NSOutlineView *outlineView;

@property (nonatomic) BOOL enabled;

@end

@implementation HBPresetsViewController

- (instancetype)initWithPresetManager:(HBPresetsManager *)presetManager
{
    self = [super initWithNibName:@"Presets" bundle:nil];
    if (self)
    {
        _presets = [presetManager retain];
        _expandedNodes = [[NSArray arrayWithArray:[[NSUserDefaults standardUserDefaults]
                                                   objectForKey:@"HBPreviewViewExpandedStatus"]] mutableCopy];
    }
    return self;
}

- (void)dealloc
{
    self.presets = nil;
    self.dragNodesArray = nil;
    self.expandedNodes = nil;
    
    [super dealloc];
}

- (void)loadView
{
    [super loadView];

    // drag and drop support
	[self.outlineView registerForDraggedTypes:@[kHandBrakePresetPBoardType]];

    // Re-expand the items
    [self expandNodes:[self.treeController.arrangedObjects childNodes]];

    [self deselect];
}

- (BOOL)validateUserInterfaceItem:(id < NSValidatedUserInterfaceItem >)anItem
{
    SEL action = anItem.action;

    if (action == @selector(setDefault:))
    {
        if (![[[self.treeController selectedObjects] firstObject] isLeaf])
        {
            return NO;
        }
    }

    return YES;
}

#pragma mark - HBViewValidation methods

- (void)setUIEnabled:(BOOL)flag
{
    self.enabled = flag;
}

#pragma mark - UI Methods

- (IBAction)clicked:(id)sender
{
    if (self.delegate && [[self.treeController.selectedObjects firstObject] isLeaf] && self.enabled)
    {
        [self.delegate selectionDidChange];
    }
}

- (IBAction)addNewPreset:(id)sender
{
    if (self.delegate)
    {
        [self.delegate showAddPresetPanel:sender];
    }
}

- (IBAction)deletePreset:(id)sender
{
    if ([self.treeController canRemove])
    {
        // Save the current selection path and apply it again after the deletion
        NSIndexPath *currentSelection = [self.treeController selectionIndexPath];
        /* Alert user before deleting preset */
        NSAlert *alert = [NSAlert alertWithMessageText:@"Warning!"
                                         defaultButton:@"OK"
                                       alternateButton:@"Cancel"
                                           otherButton:nil
                             informativeTextWithFormat:@"Are you sure that you want to delete the selected preset?"];
        [alert setAlertStyle:NSCriticalAlertStyle];

        NSInteger status = [alert runModal];

        if (status == NSAlertDefaultReturn)
        {
            [self.presets deletePresetAtIndexPath:[self.treeController selectionIndexPath]];
        }
        [self.treeController setSelectionIndexPath:currentSelection];
    }
}

- (IBAction)insertFolder:(id)sender
{
    NSIndexPath *selectionIndexPath = [self.treeController selectionIndexPath];
    if (!selectionIndexPath || [[[self.treeController selectedObjects] firstObject] isBuiltIn])
    {
        selectionIndexPath = [NSIndexPath indexPathWithIndex:self.presets.root.children.count];
    }

    HBPreset *node = [[HBPreset alloc] initWithFolderName:@"New Folder" builtIn:NO];
    [self.treeController insertObject:node atArrangedObjectIndexPath:selectionIndexPath];
    [node autorelease];
}

- (IBAction)setDefault:(id)sender
{
    HBPreset *selectedNode = [[self.treeController selectedObjects] firstObject];
    if ([[selectedNode valueForKey:@"isLeaf"] boolValue])
    {
        self.presets.defaultPreset = selectedNode;
    }
}

- (void)deselect
{
    [self.treeController setSelectionIndexPath:nil];
}

- (void)selectPreset:(HBPreset *)preset
{
    NSIndexPath *idx = [self.presets indexPathOfPreset:preset];

    if (idx)
    {
        [self.treeController setSelectionIndexPath:idx];
        [self clicked:self];
    }
}

- (HBPreset *)selectedPreset
{
    HBPreset *selectedNode = [[self.treeController selectedObjects] firstObject];
    if ([[selectedNode valueForKey:@"isLeaf"] boolValue])
    {
        return selectedNode;
    }
    else
    {
        return self.presets.defaultPreset;
    }
}

- (IBAction)updateBuiltInPresets:(id)sender
{
    [self.presets generateBuiltInPresets];

    // Re-expand the items
    [self expandNodes:[self.treeController.arrangedObjects childNodes]];
}

#pragma mark - Added Functionality (optional)

/* We use this to provide tooltips for the items in the presets outline view */
- (NSString *)outlineView:(NSOutlineView *)fPresetsOutlineView
           toolTipForCell:(NSCell *)cell
                     rect:(NSRectPointer)rect
              tableColumn:(NSTableColumn *)tc
                     item:(id)item
            mouseLocation:(NSPoint)mouseLocation
{
    return [[item representedObject] presetDescription];
}

/* Use to customize the font and display characteristics of the title cell */
- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
    NSColor *fontColor;
    
    if ([self.outlineView selectedRow] == [self.outlineView rowForItem:item])
    {
        fontColor = [NSColor blackColor];
    }
    else
    {
        if ([[item representedObject] isBuiltIn])
        {
            fontColor = [NSColor blueColor];
        }
        else // User created preset, use a black font
        {
            fontColor = [NSColor blackColor];
        }
    }

    [cell setTextColor:fontColor];
}

#pragma mark - Expanded node persitence methods

- (void)expandNodes:(NSArray *)childNodes
{
    for (id node in childNodes)
    {
        [self expandNodes:[node childNodes]];
        if ([self.expandedNodes containsObject:@([[node representedObject] hash])])
            [self.outlineView expandItem:node expandChildren:YES];
    }
}

- (void)outlineViewItemDidExpand:(NSNotification *)notification
{
    HBPreset *node = [[[notification userInfo] valueForKey:@"NSObject"] representedObject];
    if (![self.expandedNodes containsObject:@(node.hash)])
    {
        [self.expandedNodes addObject:@(node.hash)];
        [[NSUserDefaults standardUserDefaults] setObject:self.expandedNodes forKey:@"HBPreviewViewExpandedStatus"];
    }
}

- (void)outlineViewItemDidCollapse:(NSNotification *)notification
{
    HBPreset *node = [[[notification userInfo] valueForKey:@"NSObject"] representedObject];
    [self.expandedNodes removeObject:@(node.hash)];
    [[NSUserDefaults standardUserDefaults] setObject:self.expandedNodes forKey:@"HBPreviewViewExpandedStatus"];
}

#pragma mark - Drag & Drops

/**
 *  draggingSourceOperationMaskForLocal <NSDraggingSource override>
 */
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal
{
	return NSDragOperationMove;
}

/**
 *  outlineView:writeItems:toPasteboard
 */
- (BOOL)outlineView:(NSOutlineView *)ov writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard
{
    // Return no if we are trying to drag a built-in preset
    for (id item in items) {
        if ([[item representedObject] isBuiltIn])
            return NO;
    }

    [pboard declareTypes:@[kHandBrakePresetPBoardType] owner:self];

	// keep track of this nodes for drag feedback in "validateDrop"
	self.dragNodesArray = items;

	return YES;
}

/**
 *	outlineView:validateDrop:proposedItem:proposedChildrenIndex:
 *
 *	This method is used by NSOutlineView to determine a valid drop target.
 */
 - (NSDragOperation)outlineView:(NSOutlineView *)ov
                  validateDrop:(id <NSDraggingInfo>)info
                  proposedItem:(id)item
            proposedChildIndex:(NSInteger)index
{
	NSDragOperation result = NSDragOperationNone;

	if (!item)
	{
        if (index == 0)
        {
            // don't allow to drop on top
            result = NSDragOperationNone;
        }
        else
        {
            // no item to drop on
            result = NSDragOperationGeneric;
        }
	}
	else
	{
        if (index == -1 || [[item representedObject] isBuiltIn] || [self.dragNodesArray containsObject:item])
        {
            // don't allow dropping on a child
            result = NSDragOperationNone;
        }
        else
        {
            // drop location is a container
            result = NSDragOperationMove;
        }
	}

	return result;
}

/**
 *	handleInternalDrops:pboard:withIndexPath:
 *
 *	The user is doing an intra-app drag within the outline view.
 */
- (void)handleInternalDrops:(NSPasteboard *)pboard withIndexPath:(NSIndexPath *)indexPath
{
	// user is doing an intra app drag within the outline view:
	NSArray *newNodes = self.dragNodesArray;

	// move the items to their new place (we do this backwards, otherwise they will end up in reverse order)
	NSInteger idx;
	for (idx = ([newNodes count] - 1); idx >= 0; idx--)
	{
		[self.treeController moveNode:newNodes[idx] toIndexPath:indexPath];

        // Call manually this because the NSTreeController doesn't call
        // the KVC accessors method for the root node.
        if (indexPath.length == 1)
        {
            [self.presets performSelector:@selector(nodeDidChange)];
        }
	}

	// keep the moved nodes selected
	NSMutableArray *indexPathList = [NSMutableArray array];
    for (NSUInteger i = 0; i < [newNodes count]; i++)
	{
		[indexPathList addObject:[newNodes[i] indexPath]];
	}
	[self.treeController setSelectionIndexPaths: indexPathList];
}

/**
 *	outlineView:acceptDrop:item:childIndex
 *
 *	This method is called when the mouse is released over an outline view that previously decided to allow a drop
 *	via the validateDrop method. The data source should incorporate the data from the dragging pasteboard at this time.
 *	'index' is the location to insert the data as a child of 'item', and are the values previously set in the validateDrop: method.
 *
 */
- (BOOL)outlineView:(NSOutlineView *)ov acceptDrop:(id <NSDraggingInfo>)info item:(id)targetItem childIndex:(NSInteger)index
{
	// note that "targetItem" is a NSTreeNode proxy
	//
	BOOL result = NO;

	// find the index path to insert our dropped object(s)
	NSIndexPath *indexPath;
	if (targetItem)
	{
		// drop down inside the tree node:
		// feth the index path to insert our dropped node
		indexPath = [[targetItem indexPath] indexPathByAddingIndex:index];
	}
	else
	{
		// drop at the top root level
		if (index == -1)	// drop area might be ambibuous (not at a particular location)
			indexPath = [NSIndexPath indexPathWithIndex:self.presets.root.children.count]; // drop at the end of the top level
		else
			indexPath = [NSIndexPath indexPathWithIndex:index]; // drop at a particular place at the top level
	}

	NSPasteboard *pboard = [info draggingPasteboard];	// get the pasteboard

	// check the dragging type -
	if ([pboard availableTypeFromArray:@[kHandBrakePresetPBoardType]])
	{
		// user is doing an intra-app drag within the outline view
		[self handleInternalDrops:pboard withIndexPath:indexPath];
		result = YES;
	}

	return result;
}

@end
