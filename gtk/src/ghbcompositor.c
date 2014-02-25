/* "Borrowed" from: */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <config.h>
#include <stdlib.h>
#include "ghbcompat.h"
#include "ghbcompositor.h"

enum {
    PROP_0,
};

enum {
    CHILD_PROP_0,
    CHILD_PROP_Z_POS,
    CHILD_PROP_OPACITY
};

#define GHB_COMPOSITOR_GET_PRIVATE(obj)  G_TYPE_INSTANCE_GET_PRIVATE((obj), GHB_TYPE_COMPOSITOR, GhbCompositorPrivate)

static void     ghb_compositor_finalize      (GObject        *object);
static void     ghb_compositor_realize       (GtkWidget      *widget);
static void     ghb_compositor_unrealize     (GtkWidget      *widget);
static void     ghb_compositor_size_allocate (GtkWidget      *widget,
                                             GtkAllocation   *allocation);
static gboolean ghb_compositor_draw          (GtkWidget      *widget,
                                             cairo_t         *cr);
static void ghb_compositor_get_preferred_width(GtkWidget      *widget,
                                             gint            *minimum_size,
                                             gint            *natural_size);
static void ghb_compositor_get_preferred_height(GtkWidget      *widget,
                                             gint            *minimum_size,
                                             gint            *natural_size);
static void     ghb_compositor_set_property  (GObject        *object,
                                             guint            prop_id,
                                             const GValue    *value,
                                             GParamSpec      *pspec);
static void     ghb_compositor_get_property  (GObject        *object,
                                             guint            prop_id,
                                             GValue          *value,
                                             GParamSpec      *pspec);
static void     ghb_compositor_add          (GtkContainer    *container,
                                             GtkWidget       *widget);
static void     ghb_compositor_remove       (GtkContainer    *container,
                                             GtkWidget       *widget);
static void     ghb_compositor_forall       (GtkContainer    *container,
                                             gboolean         include_internals,
                                             GtkCallback      callback,
                                             gpointer         data);
static GType    ghb_compositor_child_type   (GtkContainer    *container);
static void     ghb_compositor_set_child_property  (GtkContainer *container,
                                             GtkWidget       *child,
                                             guint            prop_id,
                                             const GValue    *value,
                                             GParamSpec      *pspec);
static void     ghb_compositor_get_child_property  (GtkContainer *container,
                                             GtkWidget       *child,
                                             guint            prop_id,
                                             GValue          *value,
                                             GParamSpec      *pspec);

G_DEFINE_TYPE (GhbCompositor, ghb_compositor, GTK_TYPE_BIN)

static void
ghb_compositor_class_init (GhbCompositorClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
    GtkContainerClass *container_class = GTK_CONTAINER_CLASS (class);

    gobject_class->finalize = ghb_compositor_finalize;

    gobject_class->set_property = ghb_compositor_set_property;
    gobject_class->get_property = ghb_compositor_get_property;

    widget_class->draw = ghb_compositor_draw;
    widget_class->get_preferred_width = ghb_compositor_get_preferred_width;
    widget_class->get_preferred_height = ghb_compositor_get_preferred_height;
    widget_class->size_allocate = ghb_compositor_size_allocate;

    widget_class->realize = ghb_compositor_realize;
    widget_class->unrealize = ghb_compositor_unrealize;

    container_class->add = ghb_compositor_add;
    container_class->remove = ghb_compositor_remove;
    container_class->forall = ghb_compositor_forall;
    container_class->child_type = ghb_compositor_child_type;
    container_class->set_child_property = ghb_compositor_set_child_property;
    container_class->get_child_property = ghb_compositor_get_child_property;

    gtk_container_class_install_child_property (container_class,
                        CHILD_PROP_Z_POS,
                        g_param_spec_uint ("z-pos",
                            "Position in Z-List",
                            "Sets the blending order of the child.",
                            0, 65535, 0,
                            G_PARAM_READWRITE));

    gtk_container_class_install_child_property (container_class,
                        CHILD_PROP_OPACITY,
                        g_param_spec_double ("opacity",
                            "Opacity",
                            "Sets the opacity of the child.",
                            0.0, 1.0, 1.0,
                            G_PARAM_READWRITE));

}

static GType
ghb_compositor_child_type(GtkContainer *container)
{
    return GTK_TYPE_WIDGET;
}

static void
ghb_compositor_get_property (
    GObject    *object,
    guint       prop_id,
    GValue     *value,
    GParamSpec *pspec)
{
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
ghb_compositor_set_property (
    GObject      *object,
    guint         prop_id,
    const GValue *value,
    GParamSpec   *pspec)
{
    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static gint
zsort(gconstpointer a, gconstpointer b)
{
    GhbCompositorChild *cca, *ccb;

    cca = (GhbCompositorChild*)a;
    ccb = (GhbCompositorChild*)b;

    return (cca->z_pos - ccb->z_pos);
}

static void
ghb_compositor_set_child_property(
    GtkContainer *container,
    GtkWidget    *child,
    guint         prop_id,
    const GValue *value,
    GParamSpec   *pspec)
{
    GhbCompositor *compositor;
    GhbCompositorChild *cc;
    GList *link;

    compositor = GHB_COMPOSITOR(container);

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (cc->widget == child)
            break;
    }
    if (link == NULL)
    {
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID(container, prop_id, pspec);
        return;
    }

    switch (prop_id)
    {
    case CHILD_PROP_Z_POS:
    {
        cc->z_pos = g_value_get_uint(value);
        compositor->children = g_list_sort(compositor->children, zsort);
    } break;
    case CHILD_PROP_OPACITY:
    {
        cc->opacity = g_value_get_double(value);
    } break;
    default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID(container, prop_id, pspec);
        break;
    }

    if (gtk_widget_get_visible (child) &&
        gtk_widget_get_visible (GTK_WIDGET(compositor)))
    {
        gtk_widget_queue_resize (child);
    }

}

static void
ghb_compositor_get_child_property(
    GtkContainer *container,
    GtkWidget    *child,
    guint         prop_id,
    GValue       *value,
    GParamSpec   *pspec)
{
    GhbCompositor *compositor;
    GhbCompositorChild *cc;
    GList *link;

    compositor = GHB_COMPOSITOR(container);

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (cc->widget == child)
            break;
    }
    if (link == NULL)
    {
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID(container, prop_id, pspec);
        return;
    }

    switch (prop_id)
    {
    case CHILD_PROP_Z_POS:
    {
        g_value_set_uint(value, cc->z_pos);
    } break;
    case CHILD_PROP_OPACITY:
    {
        g_value_set_double(value, cc->opacity);
    } break;
    default:
        GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID(container, prop_id, pspec);
        break;
    }
}

static void
ghb_compositor_init (GhbCompositor *compositor)
{
    gtk_widget_set_has_window(GTK_WIDGET(compositor), TRUE);
}

GtkWidget*
ghb_compositor_new (void)
{
    return GTK_WIDGET(g_object_new (GHB_TYPE_COMPOSITOR, NULL));
}

#if 0
static void
showtype(const gchar *msg, GtkWidget *widget)
{
    GType type;
    gchar *str;

    type = G_OBJECT_TYPE(widget);
    if (type == GTK_TYPE_DRAWING_AREA)
        str = "drawing area";
    else if (type == GTK_TYPE_ALIGNMENT)
        str = "alignment";
    else if (type == GTK_TYPE_EVENT_BOX)
        str = "event box";
    else if (type == GTK_TYPE_EVENT_BOX)
        str = "event box";
    else
        str = "unknown";
    g_message("%s: %s", msg, str);
}
#endif

static void
find_drawables(GtkWidget *widget, gpointer data)
{
    GList *drawables, **pdrawables;
    pdrawables = (GList**)data;
    drawables = *pdrawables;

    if (gtk_widget_get_has_window(widget))
    {
        drawables = g_list_append(drawables, widget);
        *pdrawables = drawables;
        return;
    }
    if (GTK_IS_CONTAINER(widget))
    {
        gtk_container_forall(GTK_CONTAINER(widget), find_drawables, data);
    }
}


/**
 * ghb_compositor_zlist_insert:
 * @compositor: a #GhbCompositor
 * @child: widgets to insert
 * @z_pos: position
 * @opacity: global opacity for this widget
 *
 * Insert in the given position of the zlist in the compositor.
 * All children in the zlist must have associated GdkDrawable's
 * This means they must be GtkDrawingArea or GtkEventBox
 *
 **/
void
ghb_compositor_zlist_insert (
    GhbCompositor *compositor,
    GtkWidget *child,
    gint z_pos,
    gdouble opacity)
{
    GhbCompositorChild *cc;
    GdkDisplay *display;

    g_return_if_fail (GHB_IS_COMPOSITOR (compositor));
    g_return_if_fail (GTK_IS_WIDGET (child));
    g_return_if_fail (gtk_widget_get_parent(child) == NULL);

    gtk_widget_set_parent(child, GTK_WIDGET(compositor));

    display = gtk_widget_get_display (GTK_WIDGET(compositor));

    cc = g_new(GhbCompositorChild, 1);
    cc->widget = child;
    cc->z_pos = z_pos;
    cc->opacity = opacity;
    cc->drawables = NULL;
    compositor->children = g_list_insert_sorted(
                                compositor->children, cc, zsort);

    if (gdk_display_supports_composite(display))
    {
        GList *link;

        cc->drawables = NULL;
        find_drawables(cc->widget, &cc->drawables);

        for (link = cc->drawables; link != NULL; link = link->next)
        {
            gtk_widget_realize(GTK_WIDGET(link->data));
            gdk_window_set_composited(gtk_widget_get_window(GTK_WIDGET(link->data)), TRUE);
        }
    }
}

static void
ghb_compositor_add(GtkContainer *container, GtkWidget *child)
{
    GhbCompositor *compositor = GHB_COMPOSITOR(container);
    GhbCompositorChild *cc;
    gint z_pos = 0;
    GList *last = g_list_last(compositor->children);

    if (last != NULL)
    {
        cc = (GhbCompositorChild*)last->data;
        z_pos = cc->z_pos + 1;
    }
    ghb_compositor_zlist_insert(compositor, child, z_pos, 1.0);
}

static void
ghb_compositor_remove(GtkContainer *container, GtkWidget *child)
{
    GhbCompositor *compositor = GHB_COMPOSITOR(container);
    GhbCompositorChild *cc;
    GList *link;

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (cc->widget == child)
        {
            gboolean was_visible = gtk_widget_get_visible( child );
            gtk_widget_unparent(child);
            compositor->children = g_list_remove_link(
                                            compositor->children, link);
            g_list_free(link);
            g_free(cc);

            if (was_visible && gtk_widget_get_visible (GTK_WIDGET(container)))
                gtk_widget_queue_resize(GTK_WIDGET(container));
            break;
        }
    }
}

static void
ghb_compositor_forall(
    GtkContainer *container,
    gboolean      include_internals,
    GtkCallback   callback,
    gpointer      data)
{
    GhbCompositor *compositor = GHB_COMPOSITOR (container);
    GhbCompositorChild *cc;
    GList *link, *next;

    for (link = compositor->children; link != NULL; )
    {
        // The callback may cause the link to be removed from the list.
        // So find next before calling callback
        next = link->next;
        cc = (GhbCompositorChild*)link->data;
        (*callback)(cc->widget, data);
        link = next;
    }
}

static void
ghb_compositor_finalize (GObject *object)
{
    GhbCompositor *compositor = GHB_COMPOSITOR (object);
    GhbCompositorChild *cc;
    GList *link;

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        g_list_free(cc->drawables);
        g_free(cc);
    }
    g_list_free(compositor->children);
    G_OBJECT_CLASS (ghb_compositor_parent_class)->finalize (object);
}


static void
ghb_compositor_realize (GtkWidget *widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    gint border_width;
    gboolean visible_window;

    gtk_widget_set_realized(widget, TRUE);

    border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    attributes.x = allocation.x + border_width;
    attributes.y = allocation.y + border_width;
    attributes.width = allocation.width - 2*border_width;
    attributes.height = allocation.height - 2*border_width;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget)
            | GDK_BUTTON_MOTION_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK
            | GDK_EXPOSURE_MASK
            | GDK_ENTER_NOTIFY_MASK
            | GDK_LEAVE_NOTIFY_MASK;

    visible_window = gtk_widget_get_has_window(widget);

    GdkWindow *window;
    if (visible_window)
    {
        attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
        attributes.visual = gtk_widget_get_visual (widget);
        attributes.wclass = GDK_INPUT_OUTPUT;


        window = gdk_window_new(gtk_widget_get_parent_window (widget),
                                &attributes, attributes_mask);
        gtk_widget_set_window(widget, window);
        gdk_window_set_user_data(window, widget);
    }
    else
    {
        window = gtk_widget_get_parent_window (widget);
        gtk_widget_set_window(widget, window);
        g_object_ref (window);
    }
}

static void
ghb_compositor_unrealize (GtkWidget *widget)
{
    GTK_WIDGET_CLASS (ghb_compositor_parent_class)->unrealize (widget);
}

static void
ghb_compositor_get_preferred_width(
        GtkWidget *widget,
        gint *minimum_size,
        gint *natural_size)
{
    GhbCompositor *compositor = GHB_COMPOSITOR (widget);
    GList *link;
    GhbCompositorChild *cc;
    gint width = 0;
    GtkRequisition min_size, size;

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (gtk_widget_get_visible(cc->widget))
        {
            gtk_widget_get_preferred_size(cc->widget, &min_size, &size);
            width = MAX(MAX(min_size.width, size.width), width);
        }
    }

    *minimum_size = width + gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2;
    *natural_size = width + gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2;
}

static void
ghb_compositor_get_preferred_height(
        GtkWidget *widget,
        gint *minimum_size,
        gint *natural_size)
{
    GhbCompositor *compositor = GHB_COMPOSITOR (widget);
    GList *link;
    GhbCompositorChild *cc;
    gint height = 0;
    GtkRequisition min_size, size;

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (gtk_widget_get_visible(cc->widget))
        {
            gtk_widget_get_preferred_size(cc->widget, &min_size, &size);
            height = MAX(MAX(min_size.height, size.height), height);
        }
    }

    *minimum_size = height + gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2;
    *natural_size = height + gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2;
}

static void
ghb_compositor_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    GhbCompositor *compositor;
    GtkAllocation child_allocation;
    GhbCompositorChild *cc;
    GList *link;

    gtk_widget_set_allocation(widget, allocation);
    compositor = GHB_COMPOSITOR (widget);

    if (!gtk_widget_get_has_window(widget))
    {
        child_allocation.x = allocation->x +
                         gtk_container_get_border_width(GTK_CONTAINER(widget));
        child_allocation.y = allocation->y +
                         gtk_container_get_border_width(GTK_CONTAINER(widget));
    }
    else
    {
        child_allocation.x = 0;
        child_allocation.y = 0;
    }

    child_allocation.width = MAX (allocation->width -
                 gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2, 0);
    child_allocation.height = MAX (allocation->height -
                 gtk_container_get_border_width(GTK_CONTAINER (widget)) * 2, 0);

    if (gtk_widget_get_realized(widget))
    {
        if (gtk_widget_get_has_window(widget))
        {
            gdk_window_move_resize (gtk_widget_get_window(widget),
                allocation->x + gtk_container_get_border_width(GTK_CONTAINER (widget)),
                allocation->y + gtk_container_get_border_width(GTK_CONTAINER (widget)),
                child_allocation.width,
                child_allocation.height);
        }
    }
    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        if (gtk_widget_get_realized (cc->widget))
            gtk_widget_size_allocate (cc->widget, &child_allocation);
    }
}

#if 0
static void showrects(cairo_region_t *region)
{
    cairo_rectangle_int_t rect;
    int ii;
    int count = cairo_region_num_rectangles(region);

    printf("rect count %d\n", count);
    for (ii = 0; ii < count; ii++)
    {
        cairo_region_get_rectangle(region, ii, &rect);
        printf("rect %d: %d,%d %dx%d\n",
                ii, rect.x, rect.y, rect.width, rect.height);
    }
}
#endif

static void
ghb_compositor_blend (GtkWidget *widget, cairo_t *cr)
{
    GhbCompositor *compositor = GHB_COMPOSITOR (widget);
    GList *link, *draw;
    cairo_region_t *region;
    GtkWidget *child;
    GhbCompositorChild *cc;

    if (compositor->children == NULL) return;
    /* create a cairo context to draw to the window */

    for (link = compositor->children; link != NULL; link = link->next)
    {
        cc = (GhbCompositorChild*)link->data;
        for (draw = cc->drawables; draw != NULL; draw = draw->next)
        {
            GtkAllocation child_alloc;

            /* get our child */
            child = GTK_WIDGET(draw->data);

            if (
//!gtk_cairo_should_draw_window(cr, gtk_widget_get_window(child)) ||
                !gtk_widget_get_visible(cc->widget) ||
                !gtk_widget_get_visible(child))
                continue;

            gtk_widget_get_allocation(child, &child_alloc);
            cairo_save(cr);

            /* the source data is the (composited) event box */
            gdk_cairo_set_source_window(cr, gtk_widget_get_window(child),
                                        child_alloc.x,
                                        child_alloc.y);

            cairo_rectangle_int_t rect;

            rect.x = child_alloc.x;
            rect.y = child_alloc.y;
            rect.width = child_alloc.width;
            rect.height = child_alloc.height;

            /* draw no more than our expose event intersects our child */
            region = cairo_region_create_rectangle(&rect);

            cairo_region_t *dregion = gdk_window_get_visible_region(
                                            gtk_widget_get_window(child));
            cairo_region_translate(dregion, child_alloc.x, child_alloc.y);
            cairo_region_intersect(region, dregion);
            cairo_region_destroy(dregion);

            gdk_cairo_region(cr, region);
            cairo_region_destroy(region);
            cairo_clip(cr);

            /* composite, with an opacity */
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            cairo_paint_with_alpha(cr, cc->opacity);

            cairo_restore(cr);
        }
    }
    /* we're done */
}

static gboolean
ghb_compositor_draw(GtkWidget *widget, cairo_t *cr)
{
    if (gtk_widget_is_drawable(widget))
    {
        if (gtk_widget_get_has_window(widget))
            ghb_compositor_blend (widget, cr);

    }
    GTK_WIDGET_CLASS(ghb_compositor_parent_class)->draw(widget, cr);

    return FALSE;
}
