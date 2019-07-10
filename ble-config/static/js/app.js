

//var canvas = document.querySelector('canvas');
var statusText = document.querySelector('#statusText');
//var responseText = document.querySelector('#responseText');
var button = document.querySelector('#selectButton');
//var reconnectButton = document.querySelector('#reconnectButton');

function disconnectHandler() {
  statusText.textContent += '\nHub disconnected.';
  $('#reconnectButton').prop("disabled",false);
}

function startConnect() {
  sensaurusImg.style.display = "none";
  statusText.style.display = "block";
  statusText.textContent = 'Connecting...';
  sensaurusBle.connect(() => {
    $('#selectButton').prop("disabled", false);
    $('#save').prop("disabled",true);
    $('#reconnectButton').prop("disabled",false);
    statusText.textContent += '\nHub disconnected.';
    }
  )
  .then( () =>  {
    statusText.textContent = 'Connected: Waiting for notifications...';
  })
  .then( () => {
  // Reading  Level...
  //return characteristic.readValue();
    //console.log("deb4", characteristic);
    sensaurusBle.getAllCharacteristicValues()
    .then( () => {
      statusText.textContent = 'Settings retrieved: network=' + sensaurusBle.wifiNetwork + ', hub id=' + sensaurusBle.hubId + ' ...';
      console.log('Properties retrieved: network=' + sensaurusBle.wifiNetwork + '...');
      //console.log($('#wifiNetwork'));
      $('#wifiNetwork').val(sensaurusBle.wifiNetwork);
      $('#wifiPassword').val(sensaurusBle.wifiPassword);
      $('#ownerId').val(sensaurusBle.ownerId);
      $('#hubId').val(sensaurusBle.hubId);
      //Object.observe(sensaurusBle._connected, somethingChanged);
      $('#selectButton').prop("disabled",false);
      $('#save').prop("disabled",false);

    });
  })
  .catch(error => {
    console.log("startConnect error", error)
    statusText.textContent = error;
  });

}

// watch connect state
//var beingWatched = {};
// Define callback function to get notified on changes
//function somethingChanged(changes) {
//    // do something
//    console.log("_connected changed: ", sensaurusBle._connected, changes);
//}

function saveSettings() {
  sensaurusBle.wifiNetwork = $('#wifiNetwork').val();
  sensaurusBle.wifiPassword = $('#wifiPassword').val();
  sensaurusBle.ownerId = $('#ownerId').val();
  sensaurusBle.hubId = $('#hubId').val();
  sensaurusBle.thingCrt = $('#thingCrt').val();
  sensaurusBle.thingPrivateKey = $('#thingPrivateKey').val();
  sensaurusBle.saveSettings()
  .then( () => {
    sensaurusBle.sendCmd("bleExit")
    .catch(error => {
      console.log("Reboot command failed", error)
      statusText.textContent = error;
    });
    var msg = "Settings saved. Reboot command was sent to the hub.";
    console.log(msg)
    statusText.textContent = msg;
  })
  .catch(error => {
    console.log("save settings failed: ", error)
    statusText.textContent = error;
  });
}

function clear() {
  //responseText.textContent = "";
}

function formatTime(dt) {
  var ts = dt.format("dd/MM/yyyy HH:mm:ss fff");
  return ts;
}

button.addEventListener('click', function() {
  startConnect();
});


$( "#reconnectButton" ).click(function() {
  $('#reconnectButton').prop("disabled",true);
  statusText.textContent = 'Re-connecting...';
  sensaurusBle.reconnect()
  .then( () => {
  // Reading  Level...
  //return characteristic.readValue();
    //console.log("deb4", characteristic);
    statusText.textContent = 'Retrieving settings...';
    sensaurusBle.getAllCharacteristicValues()
    .then( () => {
      statusText.textContent = 'Settings retrieved: network=' + sensaurusBle.wifiNetwork + ', hub id=' + sensaurusBle.hubId + ' ...';
      console.log('Properties retrieved: network=' + sensaurusBle.wifiNetwork + '...');
      //console.log($('#wifiNetwork'));
      $('#wifiNetwork').val(sensaurusBle.wifiNetwork);
      $('#wifiPassword').val(sensaurusBle.wifiPassword);
      $('#ownerId').val(sensaurusBle.ownerId);
      $('#hubId').val(sensaurusBle.hubId);
      //Object.observe(sensaurusBle._connected, somethingChanged);
      $('#selectButton').prop("disabled",false);
      $('#save').prop("disabled",false);
      $('#reconnectButton').prop("disabled",true);
    });
  })
  .catch(error => {
    console.log("reconnect error", error)
    $('#selectButton').prop("disabled",false);
    $('#reconnectButton').prop("disabled",true);
    statusText.textContent = error;
  });
});
