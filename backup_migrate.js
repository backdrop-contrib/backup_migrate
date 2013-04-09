
(function($) {

  Drupal.backup_migrate = {
    callbackURL : "",  
    autoAttach  : function() {
      if (Drupal.settings.backup_migrate !== undefined) { 
        if (Drupal.settings.backup_migrate.dependents !== undefined) {
          for (key in Drupal.settings.backup_migrate.dependents) {
            info = Drupal.settings.backup_migrate.dependents[key];
            dependent = $('#edit-' + info['dependent']);
            for (key in info['dependencies']) {
              $('[name="' + key + '"]').each(function() {
                var dependentval = info['dependencies'][key];
                var dependency = $(this);
                (function(dependent, dependency) {
                  var checkval = function(inval) {
                    if (dependency.attr('type') == 'radio') {
                      var val = $('[name="' + dependency.attr('name') + '"]:checked').val();
                      return val == inval;
                    }
                    else if (dependency.attr('type') == 'checkbox') {
                      return dependency.attr('checked') && inval == dependency.val();
                    }
                    else {
                      return dependency.val() == inval;
                    }
                    return false;
                  };
                  if (!checkval(dependentval)) {
                    // Hide doesn't work inside collapsed fieldsets.
                    dependent.css('display', 'none');
                  }
                  dependency.bind('load change click keypress focus', function() {
                    if (checkval(dependentval)) {
                      dependent.slideDown();
                    }
                    else {
                      dependent.slideUp();
                    }
                  }).load();
                })(dependent, dependency);
              });
            }
          }
          for (key in Drupal.settings.backup_migrate.destination_selectors) {
            var info = Drupal.settings.backup_migrate.destination_selectors[key];
            (function(info) {
              var selector = $('#' + info['destination_selector']);
              var copy = $('#' + info['copy'])
              var copy_selector = $('#' + info['copy_destination_selector']);
              var copy_selector_options = {};

              // Store a copy of the secondary selector options.
              copy_selector.find('optgroup').each(function() {
                var label = $(this).attr('label');
                copy_selector_options[label] = [];
                $(this).find('option').each(function() {
                  copy_selector_options[label].push(this); 
                });
                $(this).remove();
              })

              // Assign an action to the main selector to modify the secondary selector
              selector.each(function() {
                $(this).bind('load change click keypress focus', function() {
                  var group = $(this).find('option[value=' + $(this).val() + ']').parents('optgroup').attr('label');
                  if (group) {
                    copy.parent().find('.backup-migrate-destination-copy-label').text(info['labels'][group]);
                    copy_selector.empty();
                    for (var key in copy_selector_options) {
                      if (key != group) {
                        copy_selector.append(copy_selector_options[key]);
                        console.log($('#' + info['copy_destination_selector']).get(0));
                      }
                    }
                  }
                }).load();
              });
            })(info);
          }

        }

        $('#edit-filters-exclude-tables').after('<div class="description backup-migrate-checkbox-link"><a href="javascript:Drupal.backup_migrate.selectToCheckboxes(\''+ 'exclude_tables' +'\');">'+ Drupal.settings.backup_migrate.checkboxLinkText +'</a></div>');
        $('#edit-filters-nodata-tables').after('<div class="description backup-migrate-checkbox-link"><a href="javascript:Drupal.backup_migrate.selectToCheckboxes(\''+ 'nodata_tables' +'\');">'+ Drupal.settings.backup_migrate.checkboxLinkText +'</a></div>');
      }
    },

    processCheckboxes : function(ctxt) {
      $("input.backup-migrate-tables-checkbox", ctxt).each(function() {
        this.do_click = function() {
          if (this.checked) {
            $(this).parent().addClass('checked');
          }
          else {
            $(this).parent().removeClass('checked');
          }
        };
        $(this).bind("click", function() { this.do_click() });
        this.do_click();
      });
    },

    selectToCheckboxes : function(field) {
      var field_id = 'edit-filters-'+ field.replace('_', '-') ;
      var $select = $('#'+ field_id);
      var $checkboxes = $('<div></div>').addClass('backup-migrate-tables-checkboxes');
      $('option', $select).each(function(i) {
        $checkboxes.append('<div class="form-item"><label class="option backup-migrate-table-select"><input type="checkbox" class="backup-migrate-tables-checkbox" id="edit-'+ field_id +'-'+ this.value +'" name="'+ $select.attr('name') +'"'+ (this.selected ? 'checked="checked"' : '') +' value="'+ this.value +'"/>'+this.value+'</label></div>');
      });
      $select.parent().find('.backup-migrate-checkbox-link').remove();
      $select.before($checkboxes);
      $select.hide();
      Drupal.backup_migrate.processCheckboxes($checkboxes);
    }
  };

  Drupal.behaviors.union_responsive = {
    attach: function(context) {
      Drupal.backup_migrate.autoAttach(context);
    }
  }
})(jQuery);
