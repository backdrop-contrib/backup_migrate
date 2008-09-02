// $Id$

Drupal.backup_migrate = {
  callbackURL : "",  
  autoAttach  : function(ctxt) {
    ctxt = ctxt || document;

    if ($("#edit-save-settings", ctxt).length && !$("#edit-save-settings", ctxt).attr("checked")) {
      // Disable input and hide its description.
      $("div.backup-migrate-save-options", ctxt).hide();
    }

    if ($("#edit-save-settings", ctxt).length && !$("#edit-save-settings", ctxt).attr("checked")) {
      // Disable input and hide its description.
      $("div.backup-migrate-save-options", ctxt).hide();
    }

    $("#edit-save-settings", ctxt).bind("click", function() {
      if (!$("#edit-save-settings", ctxt).attr("checked")) {
        $("div.backup-migrate-save-options", ctxt).slideUp('slow');
      }
      else {
        // Save unchecked; enable input.
        $("div.backup-migrate-save-options", ctxt).slideDown('slow');
      }
    });

    $("input.backup-migrate-tables-checkbox", ctxt).bind("click", function() {
      if (this.checked) {
        $(this).parent().addClass('checked');
      }
      else {
        $(this).parent().removeClass('checked');
      }
    }).click();

    $('#edit-exclude-tables', ctxt).after('<div class="description backup-migrate-checkbox-link"><a href="javascript:Drupal.backup_migrate.select_to_checkboxes(\''+ 'exclude_tables' +'\');">'+ Drupal.settings.backup_migrate.checkbox_link_text +'</a></div>');
    $('#edit-nodata-tables', ctxt).after('<div class="description backup-migrate-checkbox-link"><a href="javascript:Drupal.backup_migrate.select_to_checkboxes(\''+ 'nodata_tables' +'\');">'+ Drupal.settings.backup_migrate.checkbox_link_text +'</a></div>');
  },
  
  select_to_checkboxes : function(field) {
    var field_id = 'edit-'+ field.replace('_', '-');
    var select = $('#'+ field_id);
    select.selectOption = function(name, value) {
      $(this).children.each(function() {
        this.value = name;
        this.selected = value;
      });
    }
    var checkboxes = $('<div></div>').addClass('backup-migrate-tables-checkboxes');
    $('option', select).each(function(i) {
      checkboxes.append('<div class="form-item"><label class="option backup-migrate-table-select"><input type="checkbox" class="backup-migrate-tables-checkbox" id="edit-'+ field_id +'-'+ this.value +'" name="'+ field +'[]"'+ (this.selected ? 'checked="checked"' : '') +' value="'+ this.value +'"/>'+this.value+'</label></div>');
    });
    $('.backup-migrate-checkbox-link').remove();
    select.before(checkboxes);
    select.remove();
    Drupal.backup_migrate.autoAttach(checkboxes);
  }
}


// Global Killswitch
if (Drupal.jsEnabled) {
  $(document).ready(Drupal.backup_migrate.autoAttach);
}
