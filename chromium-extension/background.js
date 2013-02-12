
var plugin = document.getElementById ("desktop-webapp-plugin");

function getIconUrl (info)
{
    if (!info.icons || info.icons.length == 0) {
	return "";
    }

    var largest = { size:0 };
    for (var i = 0; i < info.icons.length; i++) {
	var icon = info.icons[i];
	if (icon.size > largest.size) {
	    largest = icon;
	}
    }
    return largest.url;
}

function getImageDataURL(url, callback) {
    var img = new Image();
    img.onload = function() {
        // Convert using a <canvas> and then invoke the callback
        var canvas = document.createElement('canvas');
        canvas.width = img.width;
        canvas.height = img.height;
        var ctx = canvas.getContext("2d");
        ctx.drawImage(img, 0, 0);
        var data = canvas.toDataURL("image/png");
        callback({image:img, data:data});
    };
    // Trigger the image loading
    img.src = url;
}

/* Register event listeners for apps/extensions events */
chrome.management.onInstalled.addListener (function(info) {
    if (info.isApp) {
        console.log ("Installing Chrome app");
        var icon_url = getIconUrl(info);
        getImageDataURL(icon_url, function(result){
            /* Call NPAPI plugin */
            plugin.installChromeApp (
                info.id,
                info.name,
                info.description,
                info.appLaunchUrl,
                result.data);
        });
    }
});

chrome.management.onUninstalled.addListener (function(id) {
    console.log ("Uninstalling Chrome app");
    plugin.uninstallChromeApp (id);
});
