/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global templateEngine, window, Backbone, $*/
(function() {
  "use strict";
  window.DBSelectionView = Backbone.View.extend({
    template: templateEngine.createTemplate("dbSelectionView.ejs"),

    events: {
      "change #dbSelectionList": "changeDatabase"
    },

    initialize: function() {
                  var self = this;
      this.collection.fetch({
        success: function(){
          self.render(self.$el);
         } 
      });
      this.current = this.collection.getCurrentDatabase();
    },

    changeDatabase: function(e) {
      var changeTo = $("#dbSelectionList > option:selected").attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    render: function(el) {
      this.$el = el;
      el.html(this.template.render({
        list: this.collection,
        current: this.current
      }));
      this.delegateEvents();
      return el;
    }
  });
}());
