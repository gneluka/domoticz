define(['app'], function (app) {
    app.component('domocanHardware', {
        bindings: {
            hardware: '<'
        },
        templateUrl: 'app/hardware/setup/DomoCAN.html',
        controller: DomoCANController
    });

    function DomoCANController() {
        var $ctrl = this;

        $ctrl.$onInit = function () {
            $.devIdx = $ctrl.hardware.idx;
			$.myglobals = {
				DomoCANDevTypeStr: [],
			};

            var oTable = $('#nodestable').dataTable({
                "sDom": '<"H"lfrC>t<"F"ip>',
                "oTableTools": {
                    "sRowSelect": "single",
                },
                "aaSorting": [[0, "desc"]],
                "bSortClasses": false,
                "bProcessing": true,
                "bStateSave": true,
                "bJQueryUI": true,
                "aLengthMenu": [[25, 50, 100, -1], [25, 50, 100, "All"]],
                "iDisplayLength": 25,
                "sPaginationType": "full_numbers",
                language: $.DataTableLanguage
            });

            $('#hardwarecontent #idx').val($ctrl.hardware.idx);

			$.myglobals.DomoCANDevTypeStr[1] = "Coin Sender 0.5";
			$.myglobals.DomoCANDevTypeStr[2] = "Coin Sender 1";
			$.myglobals.DomoCANDevTypeStr[3] = "Coin Sender 2";
			$.myglobals.DomoCANDevTypeStr[4] = "Coin Sender 3";
			$.myglobals.DomoCANDevTypeStr[5] = "Coin Sender 4";
			$.myglobals.DomoCANDevTypeStr[6] = "Coin Sender 5";
			$.myglobals.DomoCANDevTypeStr[7] = "Coin Sender 6";
			$.myglobals.DomoCANDevTypeStr[8] = "Coin Counter";
			$.myglobals.DomoCANDevTypeStr[9] = "Coin Counter Total";
			$.myglobals.DomoCANDevTypeStr[10] = "Active Monitor";
			$.myglobals.DomoCANDevTypeStr[11] = "Relay Module 5 Channels";

			//Build DomoCAN device types combo
			$("#hardwarecontent #nodeparamstable #dcandevtype").html("");
			$.each($.myglobals.DomoCANDevTypeStr, function (idx, name) {
				if (name) {
					var option = $('<option />');
					option.attr('value', idx).text(name);
					$("#hardwarecontent #nodeparamstable #dcandevtype").append(option);
				}
			});

            RefreshDomoCANNodeTable();
        };

        AddDomoCANNode = function () {
            var name = $("#hardwarecontent #nodeparamstable #nodename").val();
            if (name == "") {
                ShowNotify($.t('Please enter a Name!'), 2500, true);
                return;
            }
            var domocanid = $("#hardwarecontent #nodeparamstable #nodeid").val();
            if (domocanid == "") {
                ShowNotify($.t('Please enter a DomoCAN ID!'), 2500, true);
                return;
            }
			var devtype = $('#hardwarecontent #nodeparamstable #dcandevtype').val();			

            $.ajax({
                url: "json.htm?type=command&param=domocanaddnode" +
                "&hid=" + $.devIdx +
                "&name=" + encodeURIComponent(name) +
				"&devtype=" + devtype +
                "&dcanid=" + domocanid,
                async: false,
                dataType: 'json',
                success: function (data) {
                    RefreshDomoCANNodeTable();
                },
                error: function () {
                    ShowNotify($.t('Problem Adding device!'), 2500, true);
                }
            });
        }

        DomoCANDeleteNode = function (nodeidx) {
            if ($('#updelclr #nodedelete').attr("class") == "btnstyle3-dis") {
                return;
            }
            bootbox.confirm($.t("Are you sure to remove this device?"), function (result) {
                if (result == true) {
                    $.ajax({
                        url: "json.htm?type=command&param=domocanremovenode" +
						"&hid=" + $.devIdx +
                        "&idx=" + nodeidx,
                        async: false,
                        dataType: 'json',
                        success: function (data) {
                            RefreshDomoCANNodeTable();
                        },
                        error: function () {
                            ShowNotify($.t('Problem Deleting device!'), 2500, true);
                        }
                    });
                }
            });
        }

        DomoCANClearNodes = function () {
            bootbox.confirm($.t("Are you sure to delete ALL devices?\n\nThis action can not be undone!"), function (result) {
                if (result == true) {
                    $.ajax({
                        url: "json.htm?type=command&param=domocanclearnodes" +
                        "&hid=" + $.devIdx,
                        async: false,
                        dataType: 'json',
                        success: function (data) {
                            RefreshDomoCANNodeTable();
                        }
                    });
                }
            });
        }

        DomoCANUpdateNode = function (nodeidx) {
            if ($('#updelclr #nodedelete').attr("class") == "btnstyle3-dis") {
                return;
            }

            var name = $("#hardwarecontent #nodeparamstable #nodename").val();
            if (name == "") {
                ShowNotify($.t('Please enter a Name!'), 2500, true);
                return;
            }
            var domocanid = $("#hardwarecontent #nodeparamstable #nodeid").val();
            if (nodeid == "") {
                ShowNotify($.t('Please enter a node ID!'), 2500, true);
                return;
            }
			var devtype = $('#hardwarecontent #nodeparamstable #dcandevtype').val();			

            $.ajax({
                url: "json.htm?type=command&param=domocanupdatenode" +
                "&hid=" + $.devIdx +
				"&idx=" + nodeidx +
                "&name=" + encodeURIComponent(name) +
				"&devtype=" + devtype +
                "&dcanid=" + domocanid,
				async: false,
                dataType: 'json',
                success: function (data) {
                    RefreshDomoCANNodeTable();
                },
                error: function () {
                    ShowNotify($.t('Problem Updating Node!'), 2500, true);
                }
            });
        }

        RefreshDomoCANNodeTable = function () {
            $('#modal').show();
            $('#updelclr #nodeupdate').attr("class", "btnstyle3-dis");
            $('#updelclr #nodedelete').attr("class", "btnstyle3-dis");
            $("#hardwarecontent #nodeparamstable #nodename").val("");
            $("#hardwarecontent #nodeparamstable #nodeid").val("");

            var oTable = $('#nodestable').dataTable();
            oTable.fnClearTable();

            $.ajax({
                url: "json.htm?type=command&param=domocangetnodes&hid=" + $.devIdx,
                async: false,
                dataType: 'json',
                success: function (data) {
                    if (typeof data.result != 'undefined') {
                        $.each(data.result, function (i, item) {
                            var addId = oTable.fnAddData({
                                "DT_RowId": item.idx,
                                "Name": item.Name,
                                "Dev_Type": item.DomoCANDevType,
                                "DomoCAN_ID": item.DomoCANID,
                                "0": item.idx,
                                "1": item.Name,
                                "2": $.myglobals.DomoCANDevTypeStr[item.DomoCANDevType],
                                "3": item.DomoCANID
                            });
                        });
                    }
                }
            });

            /* Add a click handler to the rows - this could be used as a callback */
            $("#nodestable tbody").off();
            $("#nodestable tbody").on('click', 'tr', function () {
                $('#updelclr #nodedelete').attr("class", "btnstyle3-dis");
                if ($(this).hasClass('row_selected')) {
                    $(this).removeClass('row_selected');
                    $('#updelclr #nodeupdate').attr("class", "btnstyle3-dis");
                    $("#hardwarecontent #nodeparamstable #nodename").val("");
                    $("#hardwarecontent #nodeparamstable #nodeid").val("");
                }
                else {
                    var oTable = $('#nodestable').dataTable();
                    oTable.$('tr.row_selected').removeClass('row_selected');
                    $(this).addClass('row_selected');
                    $('#updelclr #nodeupdate').attr("class", "btnstyle3");
                    var anSelected = fnGetSelected(oTable);
                    if (anSelected.length !== 0) {
                        var data = oTable.fnGetData(anSelected[0]);
                        var idx = data["DT_RowId"];
                        $("#updelclr #nodeupdate").attr("href", "javascript:DomoCANUpdateNode(" + idx + ")");
                        $('#updelclr #nodedelete').attr("class", "btnstyle3");
                        $("#updelclr #nodedelete").attr("href", "javascript:DomoCANDeleteNode(" + idx + ")");
                        $("#hardwarecontent #nodeparamstable #nodename").val(data["1"]);
                        $("#hardwarecontent #nodeparamstable #nodeid").val(data["3"]);
						$('#hardwarecontent #nodeparamstable #dcandevtype').val(data["Dev_Type"]);
                    }
                }
            });

            $('#modal').hide();
        }
    }
});
