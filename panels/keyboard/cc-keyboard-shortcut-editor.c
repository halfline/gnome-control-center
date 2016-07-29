/* cc-keyboard-shortcut-editor.h
 *
 * Copyright (C) 2016 Endless, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 */

#include <glib-object.h>
#include <glib/gi18n.h>

#include "cc-keyboard-shortcut-editor.h"
#include "keyboard-shortcuts.h"

/*
 * Workaround to stop receiving a stray Meta modifier.
 */
#define ALL_ACCELS_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)

struct _CcKeyboardShortcutEditor
{
  GtkDialog           parent;

  GtkWidget          *add_button;
  GtkWidget          *cancel_button;
  GtkWidget          *command_entry;
  GtkWidget          *custom_shortcut_accel_label;
  GtkWidget          *edit_button;
  GtkWidget          *headerbar;
  GtkWidget          *name_entry;
  GtkWidget          *new_shortcut_conflict_label;
  GtkWidget          *remove_button;
  GtkWidget          *replace_button;
  GtkWidget          *reset_button;
  GtkWidget          *shortcut_accel_label;
  GtkWidget          *shortcut_conflict_label;
  GtkWidget          *stack;
  GtkWidget          *top_info_label;

  CcShortcutEditorMode mode;

  GdkDevice          *grab_device;

  CcKeyboardManager  *manager;
  CcKeyboardItem     *item;
  GBinding           *reset_item_binding;

  CcKeyboardItem     *collision_item;

  /* Custom shortcuts */
  GdkDevice          *grab_pointer;

  guint               custom_keycode;
  guint               custom_keyval;
  GdkModifierType     custom_mask;
  gboolean            custom_is_modifier;
  gboolean            edited : 1;
};

static void          command_entry_changed_cb                    (CcKeyboardShortcutEditor *self);
static void          name_entry_changed_cb                       (CcKeyboardShortcutEditor *self);

G_DEFINE_TYPE (CcKeyboardShortcutEditor, cc_keyboard_shortcut_editor, GTK_TYPE_DIALOG)

enum
{
  PROP_0,
  PROP_KEYBOARD_ITEM,
  PROP_MANAGER,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static void
apply_custom_item_fields (CcKeyboardShortcutEditor *self,
                          CcKeyboardItem           *item)
{
  /* Only setup the binding when it was actually edited */
  if (self->edited)
    {
      gchar *binding;

      item->keycode = self->custom_keycode;
      item->keyval = self->custom_keyval;
      item->mask = self->custom_mask;

      if (item->keycode == 0 && item->keyval == 0 && item->mask == 0)
        binding = g_strdup ("");
      else
        binding = gtk_accelerator_name_with_keycode (NULL,
                                                     item->keyval,
                                                     item->keycode,
                                                     item->mask);

      g_object_set (G_OBJECT (item), "binding", binding, NULL);

      g_free (binding);
    }

  /* Set the keyboard shortcut name and command for custom entries */
  if (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    {
      g_settings_set_string (item->settings, "name", gtk_entry_get_text (GTK_ENTRY (self->name_entry)));
      g_settings_set_string (item->settings, "command", gtk_entry_get_text (GTK_ENTRY (self->command_entry)));
    }
}

static void
clear_custom_entries (CcKeyboardShortcutEditor *self)
{
  g_signal_handlers_block_by_func (self->command_entry, command_entry_changed_cb, self);
  g_signal_handlers_block_by_func (self->name_entry, name_entry_changed_cb, self);

  gtk_entry_set_text (GTK_ENTRY (self->name_entry), "");
  gtk_entry_set_text (GTK_ENTRY (self->command_entry), "");

  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label), "");
  gtk_label_set_label (GTK_LABEL (self->new_shortcut_conflict_label), "");
  gtk_label_set_label (GTK_LABEL (self->shortcut_conflict_label), "");

  self->custom_keycode = 0;
  self->custom_keyval = 0;
  self->custom_mask = 0;
  self->custom_is_modifier = TRUE;
  self->edited = FALSE;

  self->collision_item = NULL;

  g_signal_handlers_unblock_by_func (self->command_entry, command_entry_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->name_entry, name_entry_changed_cb, self);
}

static gboolean
is_custom_shortcut (CcKeyboardShortcutEditor *self)
{
  return g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "custom");
}

static void
grab_seat (CcKeyboardShortcutEditor *self,
           GdkEvent                 *event)
{
  GdkGrabStatus status;
  GdkDevice *pointer;
  GdkDevice *device;
  GdkWindow *window;

  if (!event)
    event = gtk_get_current_event ();

  device = gdk_event_get_device (event);
  window = gtk_widget_get_window (GTK_WIDGET (self));

  if (!device || !window)
    return;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    pointer = gdk_device_get_associated_device (device);
  else
    pointer = device;

  status = gdk_seat_grab (gdk_device_get_seat (pointer),
                          window,
                          GDK_SEAT_CAPABILITY_ALL,
                          FALSE,
                          NULL,
                          event,
                          NULL,
                          NULL);

  if (status != GDK_GRAB_SUCCESS)
    return;

  self->grab_pointer = pointer;

  gtk_grab_add (GTK_WIDGET (self));
}

static void
release_grab (CcKeyboardShortcutEditor *self)
{
  if (self->grab_pointer)
    {
      gdk_seat_ungrab (gdk_device_get_seat (self->grab_pointer));
      self->grab_pointer = NULL;

      gtk_grab_remove (GTK_WIDGET (self));
    }
}

static void
update_shortcut (CcKeyboardShortcutEditor *self)
{
  if (!self->item)
    return;

  /* Setup the binding */
  apply_custom_item_fields (self, self->item);

  /* Eventually disable the conflict shortcut */
  if (self->collision_item)
    cc_keyboard_manager_disable_shortcut (self->manager, self->collision_item);

  /* Cleanup whatever was set before */
  clear_custom_entries (self);

  cc_keyboard_shortcut_editor_set_item (self, NULL);
}

static GtkShortcutLabel*
get_current_shortcut_label (CcKeyboardShortcutEditor *self)
{
  if (is_custom_shortcut (self))
    return GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label);

  return GTK_SHORTCUT_LABEL (self->shortcut_accel_label);
}

static void
set_collision_headerbar (CcKeyboardShortcutEditor *self,
                         gboolean                  has_collision)
{
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->headerbar), !has_collision);

  gtk_widget_set_visible (self->cancel_button, has_collision);
  gtk_widget_set_visible (self->replace_button, has_collision);
}

static void
setup_custom_shortcut (CcKeyboardShortcutEditor *self)
{
  GtkShortcutLabel *shortcut_label;
  CcKeyboardItem *collision_item;
  gboolean valid;
  gchar *accel;

  valid = is_valid_binding (self->custom_keyval, self->custom_mask, self->custom_keycode) &&
          gtk_accelerator_valid (self->custom_keyval, self->custom_mask) &&
          !self->custom_is_modifier;

  /* Additional checks for custom shortcuts */
  if (is_custom_shortcut (self))
    {
      valid = valid &&
              gtk_entry_get_text_length (GTK_ENTRY (self->name_entry)) > 0 &&
              gtk_entry_get_text_length (GTK_ENTRY (self->command_entry)) > 0;
    }

  gtk_widget_set_sensitive (self->add_button, valid);

  if (!valid)
    return;

  shortcut_label = get_current_shortcut_label (self);

  collision_item = cc_keyboard_manager_get_collision (self->manager,
                                                      self->item,
                                                      self->custom_keyval,
                                                      self->custom_mask,
                                                      self->custom_keycode);

  accel = gtk_accelerator_name (self->custom_keyval, self->custom_mask);


  /* Setup the accelerator label */
  gtk_shortcut_label_set_accelerator (shortcut_label, accel);

  /*
   * When the user finishes typing the new shortcut, it gets immediately
   * applied and the toggle button gets inactive.
   */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->edit_button), FALSE);

  self->edited = TRUE;

  release_grab (self);

  /*
   * Oops! Looks like the accelerator is already being used, so we
   * must warn the user and let it be very clear that adding this
   * shortcut will disable the other.
   */
  gtk_widget_set_visible (self->new_shortcut_conflict_label, collision_item != NULL);

  if (collision_item)
    {
      GtkWidget *label;
      gchar *friendly_accelerator;
      gchar *collision_text;

      friendly_accelerator = convert_keysym_state_to_string (self->custom_keyval,
                                                             self->custom_mask,
                                                             self->custom_keycode);

      collision_text = g_strdup_printf (_("%s is already being used for <b>%s</b>. If you "
                                          "replace it, %s will be disabled"),
                                        friendly_accelerator,
                                        collision_item->description,
                                        collision_item->description);

      label = is_custom_shortcut (self) ? self->new_shortcut_conflict_label : self->shortcut_conflict_label;

      gtk_label_set_markup (GTK_LABEL (label), collision_text);

      g_free (friendly_accelerator);
      g_free (collision_text);
    }

  /*
   * When there is a collision between the current shortcut and another shortcut,
   * and we're editing an existing shortcut (rather than creating a new one), setup
   * the headerbar to display "Cancel" and "Replace". Otherwise, make sure to set
   * only the close button again.
   */
  if (self->mode == CC_SHORTCUT_EDITOR_EDIT)
    set_collision_headerbar (self, collision_item != NULL);

  self->collision_item = collision_item;

  g_free (accel);
}

static void
add_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  CcKeyboardItem *item;

  item = cc_keyboard_manager_create_custom_shortcut (self->manager);

  /* Apply the custom shortcut setup at the new item */
  apply_custom_item_fields (self, item);

  /* Eventually disable the conflict shortcut */
  if (self->collision_item)
    cc_keyboard_manager_disable_shortcut (self->manager, self->collision_item);

  /* Cleanup everything once we're done */
  clear_custom_entries (self);

  cc_keyboard_manager_add_custom_shortcut (self->manager, item);

  gtk_widget_hide (GTK_WIDGET (self));
}

static void
cancel_button_clicked_cb (GtkWidget                *button,
                          CcKeyboardShortcutEditor *self)
{
  cc_keyboard_shortcut_editor_set_item (self, NULL);
  clear_custom_entries (self);

  gtk_widget_hide (GTK_WIDGET (self));
}

static void
command_entry_changed_cb (CcKeyboardShortcutEditor *self)
{
  setup_custom_shortcut (self);
}

static void
edit_custom_shortcut_button_toggled_cb (CcKeyboardShortcutEditor *self,
                                        GParamSpec               *pspec,
                                        GtkToggleButton          *button)
{
  if (gtk_toggle_button_get_active (button))
    grab_seat (self, NULL);
  else
    release_grab (self);
}

static void
name_entry_changed_cb (CcKeyboardShortcutEditor *self)
{
  setup_custom_shortcut (self);
}

static void
remove_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  gtk_widget_hide (GTK_WIDGET (self));

  cc_keyboard_manager_remove_custom_shortcut (self->manager, self->item);
}

static void
replace_button_clicked_cb (CcKeyboardShortcutEditor *self)
{
  update_shortcut (self);

  gtk_widget_hide (GTK_WIDGET (self));
}

static void
reset_item_clicked_cb (CcKeyboardShortcutEditor *self)
{
  gchar *accel;

  /* Reset first, then update the shortcut */
  cc_keyboard_manager_reset_shortcut (self->manager, self->item);

  accel = gtk_accelerator_name (self->item->keyval, self->item->mask);
  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->shortcut_accel_label), accel);

  g_free (accel);
}

static void
setup_keyboard_item (CcKeyboardShortcutEditor *self,
                     CcKeyboardItem           *item)
{
  gboolean is_custom;
  gchar *accel;
  gchar *text;

  if (!item)
    return;

  is_custom = item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
  accel = gtk_accelerator_name (item->keyval, item->mask);

  /* Headerbar */
  gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar), item->description);
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->headerbar), TRUE);

  gtk_widget_hide (self->add_button);
  gtk_widget_hide (self->cancel_button);
  gtk_widget_hide (self->replace_button);

  /* Setup the top label */
  text = g_strdup_printf (_("Enter new shortcut to change <b>%s</b>."), item->description);

  gtk_label_set_markup (GTK_LABEL (self->top_info_label), text);

  /* Accelerator labels */
  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->shortcut_accel_label), accel);
  gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label), accel);

  g_clear_pointer (&self->reset_item_binding, g_binding_unbind);
  self->reset_item_binding = g_object_bind_property (item,
                                                     "is-value-default",
                                                     self->reset_button,
                                                     "visible",
                                                     G_BINDING_DEFAULT | G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);

  /* Setup the custom entries */
  if (is_custom)
    {
      g_signal_handlers_block_by_func (self->command_entry, command_entry_changed_cb, self);
      g_signal_handlers_block_by_func (self->name_entry, name_entry_changed_cb, self);

      /* Name entry */
      gtk_entry_set_text (GTK_ENTRY (self->name_entry), item->description);
      gtk_widget_set_sensitive (self->name_entry, item->desc_editable);

      /* Command entry */
      gtk_entry_set_text (GTK_ENTRY (self->command_entry), item->command);
      gtk_widget_set_sensitive (self->command_entry, item->cmd_editable);

      gtk_widget_show (self->remove_button);

      g_signal_handlers_unblock_by_func (self->command_entry, command_entry_changed_cb, self);
      g_signal_handlers_unblock_by_func (self->name_entry, name_entry_changed_cb, self);
    }

  g_free (accel);
  g_free (text);

  /* Show the apropriate view */
  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), is_custom ? "custom" : "edit");
}

static void
cc_keyboard_shortcut_editor_finalize (GObject *object)
{
  CcKeyboardShortcutEditor *self = (CcKeyboardShortcutEditor *)object;

  g_clear_object (&self->item);
  g_clear_object (&self->manager);

  g_clear_pointer (&self->reset_item_binding, g_binding_unbind);

  G_OBJECT_CLASS (cc_keyboard_shortcut_editor_parent_class)->finalize (object);
}

static void
cc_keyboard_shortcut_editor_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_KEYBOARD_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_keyboard_shortcut_editor_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (object);

  switch (prop_id)
    {
    case PROP_KEYBOARD_ITEM:
      cc_keyboard_shortcut_editor_set_item (self, g_value_get_object (value));
      break;

    case PROP_MANAGER:
      g_set_object (&self->manager, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
cc_keyboard_shortcut_editor_key_press_event (GtkWidget   *widget,
                                             GdkEventKey *event)
{
  CcKeyboardShortcutEditor *self;
  GdkModifierType real_mask;
  gboolean editing;

  self = CC_KEYBOARD_SHORTCUT_EDITOR (widget);

  editing = !g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (self->stack)), "custom") ||
             gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->edit_button));

  if (!editing)
    return GTK_WIDGET_CLASS (cc_keyboard_shortcut_editor_parent_class)->key_press_event (widget, event);

  real_mask = event->state & gtk_accelerator_get_default_mod_mask () & ALL_ACCELS_MASK;

  /* A single Escape press cancels the editing */
  if (!event->is_modifier && real_mask == 0 && event->keyval == GDK_KEY_Escape)
    {
      self->edited = FALSE;

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->edit_button), FALSE);
      release_grab (self);

      return GDK_EVENT_STOP;
    }

  /* Backspace disables the current shortcut */
  if (!event->is_modifier && real_mask == 0 && event->keyval == GDK_KEY_BackSpace)
    {
      self->edited = TRUE;
      self->custom_keycode = 0;
      self->custom_keyval = 0;
      self->custom_mask = 0;

      if (self->item)
        apply_custom_item_fields (self, self->item);

      gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->custom_shortcut_accel_label), "");
      gtk_shortcut_label_set_accelerator (GTK_SHORTCUT_LABEL (self->shortcut_accel_label), "");

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->edit_button), FALSE);
      release_grab (self);

      self->edited = FALSE;

      return GDK_EVENT_STOP;
    }

  self->custom_is_modifier = event->is_modifier;
  self->custom_keycode = event->hardware_keycode;
  self->custom_keyval = event->keyval;
  self->custom_mask = real_mask;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  self->custom_mask &= ~GDK_LOCK_MASK;

  if (!self->grab_pointer)
    grab_seat (self, (GdkEvent*) event);

  setup_custom_shortcut (self);

  return GDK_EVENT_STOP;
}

static void
cc_keyboard_shortcut_editor_close (GtkDialog *dialog)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (dialog);

  if (self->mode == CC_SHORTCUT_EDITOR_EDIT)
    update_shortcut (self);

  GTK_DIALOG_CLASS (cc_keyboard_shortcut_editor_parent_class)->close (dialog);
}

static void
cc_keyboard_shortcut_editor_response (GtkDialog *dialog,
                                      gint       response_id)
{
  CcKeyboardShortcutEditor *self = CC_KEYBOARD_SHORTCUT_EDITOR (dialog);

  if (response_id == GTK_RESPONSE_DELETE_EVENT &&
      self->mode == CC_SHORTCUT_EDITOR_EDIT)
    {
      update_shortcut (self);
    }
}

static void
cc_keyboard_shortcut_editor_class_init (CcKeyboardShortcutEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_keyboard_shortcut_editor_finalize;
  object_class->get_property = cc_keyboard_shortcut_editor_get_property;
  object_class->set_property = cc_keyboard_shortcut_editor_set_property;

  widget_class->key_press_event = cc_keyboard_shortcut_editor_key_press_event;

  dialog_class->close = cc_keyboard_shortcut_editor_close;
  dialog_class->response = cc_keyboard_shortcut_editor_response;

  /**
   * CcKeyboardShortcutEditor:keyboard-item:
   *
   * The current keyboard shortcut being edited.
   */
  properties[PROP_KEYBOARD_ITEM] = g_param_spec_object ("keyboard-item",
                                                        "Keyboard item",
                                                        "The keyboard item being edited",
                                                        CC_TYPE_KEYBOARD_ITEM,
                                                        G_PARAM_READWRITE);

  /**
   * CcKeyboardShortcutEditor:panel:
   *
   * The current keyboard panel.
   */
  properties[PROP_MANAGER] = g_param_spec_object ("manager",
                                                  "Keyboard manager",
                                                  "The keyboard manager",
                                                  CC_TYPE_KEYBOARD_MANAGER,
                                                  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/shortcut-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, add_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, command_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, custom_shortcut_accel_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, edit_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, headerbar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, new_shortcut_conflict_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, remove_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, replace_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, reset_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, shortcut_accel_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, shortcut_conflict_label);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, stack);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardShortcutEditor, top_info_label);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, command_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, edit_custom_shortcut_button_toggled_cb);
  gtk_widget_class_bind_template_callback (widget_class, name_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, replace_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, reset_item_clicked_cb);
}

static void
cc_keyboard_shortcut_editor_init (CcKeyboardShortcutEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mode = CC_SHORTCUT_EDITOR_EDIT;
  self->custom_is_modifier = TRUE;
}

/**
 * cc_keyboard_shortcut_editor_new:
 *
 * Creates a new #CcKeyboardShortcutEditor.
 *
 * Returns: (transfer full): a newly created #CcKeyboardShortcutEditor.
 */
GtkWidget*
cc_keyboard_shortcut_editor_new (CcKeyboardManager *manager)
{
  return g_object_new (CC_TYPE_KEYBOARD_SHORTCUT_EDITOR,
                       "manager", manager,
                       "use-header-bar", 1,
                       NULL);
}

/**
 * cc_keyboard_shortcut_editor_get_item:
 * @self: a #CcKeyboardShortcutEditor
 *
 * Retrieves the current keyboard shortcut being edited.
 *
 * Returns: (transfer none)(nullable): a #CcKeyboardItem
 */
CcKeyboardItem*
cc_keyboard_shortcut_editor_get_item (CcKeyboardShortcutEditor *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self), NULL);

  return self->item;
}

/**
 * cc_keyboard_shortcut_editor_set_item:
 * @self: a #CcKeyboardShortcutEditor
 * @item: a #CcKeyboardItem
 *
 * Sets the current keyboard shortcut to be edited.
 */
void
cc_keyboard_shortcut_editor_set_item (CcKeyboardShortcutEditor *self,
                                      CcKeyboardItem           *item)
{
  g_return_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self));

  if (!g_set_object (&self->item, item))
    return;

  setup_keyboard_item (self, item);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_KEYBOARD_ITEM]);
}

CcShortcutEditorMode
cc_keyboard_shortcut_editor_get_mode (CcKeyboardShortcutEditor *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self), 0);

  return self->mode;
}

void
cc_keyboard_shortcut_editor_set_mode (CcKeyboardShortcutEditor *self,
                                      CcShortcutEditorMode      mode)
{
  gboolean is_create_mode;

  g_return_if_fail (CC_IS_KEYBOARD_SHORTCUT_EDITOR (self));

  if (self->mode == mode)
    return;

  self->mode = mode;
  is_create_mode = mode == CC_SHORTCUT_EDITOR_CREATE;

  gtk_widget_set_visible (self->new_shortcut_conflict_label, is_create_mode);

  if (mode == CC_SHORTCUT_EDITOR_CREATE)
    {
      /* Cleanup whatever was set before */
      clear_custom_entries (self);

      /* The 'Add' button is only sensitive when the shortcut is valid */
      gtk_widget_set_sensitive (self->add_button, FALSE);

      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (self->headerbar), FALSE);
      gtk_header_bar_set_title (GTK_HEADER_BAR (self->headerbar), _("Add Custom Shortcut"));

      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "custom");

      gtk_widget_show (self->add_button);
      gtk_widget_show (self->cancel_button);

      gtk_widget_hide (self->remove_button);
      gtk_widget_hide (self->replace_button);
    }
}
