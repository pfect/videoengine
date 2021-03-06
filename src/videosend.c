/*
 *  videosend - camera streaming utility 
 *  Copyright (C) 2021  Pasi Patama
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * This work is based on gstreamer example applications and references
 * indicated in README.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <gst/gst.h>
#define MAXCHAR 1000

/* Take vitals from oximeter provided data */
static char readvitals(char *str)
{
	char input[MAXCHAR];
	memset(input,0,MAXCHAR);
	FILE *fp;
	fp = fopen("/tmp/oxy", "r");
	if (fp == NULL)
	{
	  sprintf(str, "");
	} else {
		while (fgets(input, MAXCHAR, fp) != NULL)
			sprintf(str, "%s", input);
		fclose(fp);
	}
}

int
main (int argc, char *argv[])
{
	/* Take arguments */
	int usetestsource=0;
	char defaultvideodevice[]="/dev/video0";
	char *videodevice=NULL;
	char defaultidentity[]="";
	char *identity=NULL;
	char *logofilename=NULL;
	char *lefttoptext=NULL;
	char *dstip=NULL;
	char *dstport=NULL;
	char sinkstring[100];
	char *capture_x=NULL;
	char *capture_y=NULL;
	char *capture_fps=NULL;
	char capturestring[100];
	int c, index;
	opterr = 0;
	
	while ((c = getopt (argc, argv, "td:hi:c:l:a:p:x:y:f:")) != -1)
	switch (c)
	  {
	  case 't':
		usetestsource = 1;
		break;
	  case 'd':
		videodevice = optarg;
		break;
	  case 'h':
		fprintf(stderr,"Usage: -d [videodevice] \n       -t use test video source.\n");
		fprintf(stderr,"       -i [identity]\n");
		fprintf(stderr,"       -l [logo image file]\n");
		fprintf(stderr,"       -c [left top text]\n");
		fprintf(stderr,"       -a [address]\n");
		fprintf(stderr,"       -p [port]\n");
		fprintf(stderr,"       -x [capture resolution x]\n");
		fprintf(stderr,"       -y [capture resolution y]\n");
		fprintf(stderr,"       -f [capture fps]\n");
		fprintf(stderr,"\n");
		fprintf(stderr,"       You can check camera supported formats with:\n");
		fprintf(stderr,"       v4l2-ctl --list-formats-ext \n\n");
		
		
		return 1;
		break;
	  case 'x':
	    capture_x = optarg;
	    break;
	  case 'y':
	    capture_y = optarg;
	    break;
	  case 'f':
	    capture_fps = optarg;
	    break;  
	  case 'a':
	    dstip = optarg;
	    break;
	  case 'p':
	    dstport = optarg;
	    break;
	  case 'c':
		lefttoptext = optarg;
		break;
      case 'i':
		identity = optarg;
		break;
	  case 'l':
		logofilename = optarg;
		break;
	  case '?':
		if (optopt == 'd') {
		  fprintf (stderr, "Option -%c requires video device name as an argument.\n", optopt);
		  fprintf (stderr, "Example: videosend -d /dev/video0 \n");
		}
		else if (isprint (optopt))
		  fprintf (stderr, "Unknown option `-%c'.\n", optopt);
		else
		  fprintf (stderr,
				   "Unknown option character `\\x%x'.\n",
				   optopt);
		return 1;
	  default:
		
		break;
	  }

	/* Use defaults if none is provided by argument*/
	if (videodevice == NULL)
		videodevice = defaultvideodevice;
	if (identity == NULL)
		identity = defaultidentity;
	
	/* Initialize gstreamer */
	GstElement *pipeline, *logo, *source, *sink, *filter,*videoconverter,*encoder,*mux,*payload, *textlabel_left,*textlabel_right, *timelabel, *clocklabel;
	
	GstCaps *filtercaps;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;

	gst_init (&argc, &argv);

	/* Selectable source (test vs camera) */
	if ( usetestsource == 1 )
		source = gst_element_factory_make ("videotestsrc", "source"); 
	if ( usetestsource == 0 ) {
		source = gst_element_factory_make ("v4l2src", "source");
		g_object_set (G_OBJECT ( source ), "device",videodevice, NULL); 
	}
	
	GstElement *capsfilter = gst_element_factory_make("capsfilter", "camera_caps");
	
	/* Set camera caps */
	GstCaps *caps;
	if ( capture_x != NULL && capture_y != NULL && capture_fps != NULL ) 
	{
		memset(capturestring,100,0);
		sprintf(capturestring,"video/x-raw, width=%s, height=%s,framerate=%s/1",capture_x,capture_y,capture_fps);
		caps = gst_caps_from_string (capturestring);
	}
	else {
		caps = gst_caps_from_string ("video/x-raw, width=640, height=480,framerate=15/1");
	}
	g_object_set (capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
	
	/* Use logo if it was given -l [logo image file] otherwise use 
	 * 'identity' dummy element just to pass data in pipe.
	 * Example logo size: 150 x 150 
	 */
	if (logofilename == NULL)
		logo = gst_element_factory_make ("identity", NULL);
	else {
		logo = gst_element_factory_make ("gdkpixbufoverlay", NULL);
		g_object_set (G_OBJECT ( logo ), "location", logofilename, NULL);
		g_object_set (G_OBJECT ( logo ), "offset-x",275, NULL);
		g_object_set (G_OBJECT ( logo ), "offset-y",380, NULL);
		g_object_set (G_OBJECT ( logo ), "overlay-height",90, NULL);
		g_object_set (G_OBJECT ( logo ), "overlay-width",90, NULL);
	}

	/* See: gstbasetextoverlay.h for positioning enums */
	if ( lefttoptext == NULL ) 
		textlabel_left = gst_element_factory_make ("identity", NULL);
	else {
		textlabel_left = gst_element_factory_make ("textoverlay", NULL);
		g_object_set (G_OBJECT ( textlabel_left ), "text", lefttoptext, NULL);
		g_object_set (G_OBJECT ( textlabel_left ), "valignment", 2, NULL);
		g_object_set (G_OBJECT ( textlabel_left ), "halignment", 0, NULL);
		g_object_set (G_OBJECT ( textlabel_left ), "shaded-background", TRUE, NULL);
		g_object_set (G_OBJECT ( textlabel_left ), "font-desc", "Sans, 14", NULL);
	}
	
	textlabel_right = gst_element_factory_make ("textoverlay", NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "text", "", NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "valignment", 2, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "halignment", 2, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "font-desc", "Sans, 14", NULL);
	 
	timelabel = gst_element_factory_make ("timeoverlay", NULL);
	g_object_set (G_OBJECT ( timelabel ), "text", "", NULL);
	g_object_set (G_OBJECT ( timelabel ), "valignment", 1, NULL);
	g_object_set (G_OBJECT ( timelabel ), "halignment", 2, NULL);
	g_object_set (G_OBJECT ( timelabel ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( timelabel ), "line-alignment", 0, NULL);
	g_object_set (G_OBJECT ( timelabel ), "font-desc", "Sans, 14", NULL);
	
	clocklabel = gst_element_factory_make ("clockoverlay", NULL);
	g_object_set (G_OBJECT ( clocklabel ), "text", identity, NULL);
	g_object_set (G_OBJECT ( clocklabel ), "valignment", 1, NULL);
	g_object_set (G_OBJECT ( clocklabel ), "halignment", 0, NULL);
	g_object_set (G_OBJECT ( clocklabel ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( clocklabel ), "line-alignment", 0, NULL);
	g_object_set (G_OBJECT ( clocklabel ), "font-desc", "Sans, 14", NULL);
	
	videoconverter = gst_element_factory_make ("videoconvert", NULL);
	if (videoconverter == NULL)
		g_error ("Could not create 'videoconvert' element");
 
	encoder = gst_element_factory_make ("x264enc", NULL);
	if (encoder == NULL)
		g_error ("Could not create 'x264enc' element");
  
	g_object_set(G_OBJECT(encoder), "bitrate", 512, NULL);
	g_object_set(G_OBJECT(encoder), "pass", 5, NULL);
	g_object_set(G_OBJECT(encoder), "speed-preset", 6, NULL);
	g_object_set(G_OBJECT(encoder), "quantizer", 21, NULL);
	g_object_set(G_OBJECT(encoder), "tune",4, NULL); // zerolatency
	g_object_set(G_OBJECT(encoder), "byte-stream", TRUE, NULL);
	g_object_set(G_OBJECT(encoder), "threads", 4, NULL);
	g_object_set(G_OBJECT(encoder), "intra-refresh", TRUE, NULL);
   
	mux = gst_element_factory_make ("mpegtsmux", NULL);
	if (mux == NULL)
		g_error ("Could not create mpegtsmux");
	
	payload = gst_element_factory_make ("rtpmp2tpay",NULL);
	if (payload == NULL)
		g_error ("Could not create rtpmp2tpay");
	
	sink = gst_element_factory_make ("multiudpsink", NULL);
	if (sink == NULL)
		g_error ("Could not create multiudpsink");
	
	/* Sink setup */
	if ( dstip != NULL && dstport != NULL ) 
	{
		memset(sinkstring,100,0);
		sprintf(sinkstring,"%s:%s,127.0.0.1:4999",dstip,dstport);
		g_object_set(G_OBJECT(sink), "clients", sinkstring, NULL);
	} else {
		g_object_set(G_OBJECT(sink), "clients", "0.0.0.0:5000,127.0.0.1:4999", NULL);
	}
	pipeline = gst_pipeline_new ("test-pipeline");

	if (!pipeline ) {
		g_printerr ("Not all elements could be created: pipeline \n");
		return -1;
	}
	if (!source) {
		g_printerr ("Not all elements could be created: source \n");
		return -1;
	}
	if (!sink) {
		g_printerr ("Not all elements could be created: sink \n");
		return -1;
	}

	/* Build the pipeline */
	gst_bin_add_many (GST_BIN (pipeline), source,capsfilter,logo,textlabel_left,textlabel_right,timelabel,clocklabel,videoconverter, encoder,mux,payload, sink, NULL);

	if ( gst_element_link_many (source,capsfilter,logo,textlabel_left,textlabel_right,timelabel,clocklabel,videoconverter, encoder,mux,payload,sink,NULL) != TRUE) {
		g_printerr ("Elements could not be linked.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	/* Start playing */
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	/* Update display text */
	for (int i = 0; i < 12000; i++) {
		double load[3];  
		char dispbuf[100];
		memset(dispbuf,0,100);
		readvitals(dispbuf);
		g_object_set (textlabel_right, "text", dispbuf, NULL);
		g_usleep(1000*1000);
		if ( i > 10000 ) 
			i=0;
	}	
	
	/* Wait until error or EOS */
	bus = gst_element_get_bus (pipeline);
	msg =
	  gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
	  GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* Parse message */
	if (msg != NULL) {
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE (msg)) {
	  case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &err, &debug_info);
		g_printerr ("Error received from element %s: %s\n",
			GST_OBJECT_NAME (msg->src), err->message);
		g_printerr ("Debugging information: %s\n",
			debug_info ? debug_info : "none");
		g_clear_error (&err);
		g_free (debug_info);
		break;
	  case GST_MESSAGE_EOS:
		g_print ("End-Of-Stream reached.\n");
		break;
	  default:
		/* We should not reach here because we only asked for ERRORs and EOS */
		g_printerr ("Unexpected message received.\n");
		break;
	}
	gst_message_unref (msg);
	}

	/* Free resources */
	gst_object_unref (bus);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (pipeline);

	return 0;
}
