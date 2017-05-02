// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This function is called by common.js when the NaCl module is
// loaded.

var st = null;
var videoView_1=null;

function moduleDidLoad() {
  // Once we load, hide the plugin. In this example, we don't display anything
  // in the plugin, so it is fine to hide it.
  common.hideModule();

  // After the NaCl module has loaded, common.naclModule is a reference to the
  // NaCl module's <embed> element.
  //
  // postMessage sends a message to it.
  
  //---------- OSH ------------------//
    // We can add a group of dataSources and set the options
    var dataReceiverController = new OSH.DataReceiver.DataReceiverController({
        replayFactor : 1
    });

	var videoDataSource = new OSH.DataReceiver.VideoH264("H264 video ", {
		protocol: "ws",
		service: "SOS",
		endpointUrl: "localhost:8182/sensorhub/sos",
		offeringID: "urn:mysos:offering:foscam-r2",
		observedProperty: "http://sensorml.com/ont/swe/property/VideoFrame",
		startTime: "now",
		endTime: "2055-08-11T20:18:05.451Z",
		syncMasterTime: false,
		bufferingTime: 0
	});


	videoView_1 = new OSH.UI.FFMPEGView("video", {
		dataSourceId: videoDataSource.id,
		css: "video",
		cssSelected: "video-selected",
		name: "Android Video",
		useWorker: true,
		useWebWorkerTransferableData: false,
		width: 1920,
		height: 1080
	});
	st = new Date().getTime();
	dataReceiverController.addDataSource(videoDataSource);
	OSH.EventManager.fire(OSH.EventManager.EVENT.CONNECT_DATASOURCE,{dataSourcesId:[videoDataSource.id]});
    //---------- OSH END ------------------//
}

// This function is called by common.js when a message is received from the
// NaCl module.
function handleMessage(message) {
  var logEl = document.getElementById('log');
  logEl.textContent = (message.data.nbFrames/((new Date().getTime() - st)/1000)).toFixed(3)+ " fps";
  /*videoView_1.displayFrame(1920,1080, {
	  y : new Uint8Array(message.data.ybuffer),
	  u : new Uint8Array(message.data.ubuffer),
	  v : new Uint8Array(message.data.vbuffer)
  });*/
}
