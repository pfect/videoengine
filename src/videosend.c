/*
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
 * Compile:
 * 
 * gcc videosend.c -o videosend `pkg-config --cflags --libs gstreamer-1.0`
 * 
 * gstreamer video streaming, based on gstreamer example
 * 
 * Copyright Pasi Patama, 2021 
 * 
 * References:
 * 
 * https://github.com/cuilf/gstreamer_example/blob/master/gst-caps-demo.c
 * https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c
 * https://gist.github.com/beeender/d539734794606a38d4e3
 * https://github.com/crearo/gstreamer-cookbook/blob/master/C/dynamic-recording.c
 * https://gist.github.com/crearo/8c71729ed58c4c134cac44db74ea1754
 * https://stackoverflow.com/questions/34213636/add-clockoverlay-in-pipeline-correct-use-of-capsfilter
 * https://fossies.org/linux/gstreamer/tests/examples/controller/absolute-example.c
 * https://github.com/on-three/video-text-relay
 * 
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gst/gst.h>

static char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK...";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}
#define MAXCHAR 1000
static char readvitals(char *str)
{
	char input[MAXCHAR];
	memset(input,0,MAXCHAR);
	FILE *fp;
	fp = fopen("/tmp/oxy", "r");

	if (fp == NULL)
	{
	  perror("Error while opening the oxymeter file.\n");
	  exit(EXIT_FAILURE);
	}

	while (fgets(input, MAXCHAR, fp) != NULL)
        sprintf(str, "%s", input);

	fclose(fp);
}

int
main (int argc, char *argv[])
{
	GstElement *pipeline, *testsource, *source, *sink, *filter,*videoconverter,*encoder,*mux,*payload, *textlabel_left,*textlabel_right, *timelabel, *clocklabel;
	
	GstCaps *filtercaps;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;

	gst_init (&argc, &argv);

	/* Selectable source (test vs camera) */
	testsource = gst_element_factory_make ("videotestsrc", "source"); 
	source = gst_element_factory_make ("v4l2src", "source");
	
	GstElement *capsfilter = gst_element_factory_make("capsfilter", "camera_caps");
	GstCaps *caps = gst_caps_from_string ("video/x-raw, width=640, height=480,framerate=15/1");
	g_object_set (capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
	g_object_set (G_OBJECT ( source ), "device", "/dev/video0", NULL);

	/* See: gstbasetextoverlay.h for positioning enums */
	 
	textlabel_left = gst_element_factory_make ("textoverlay", NULL);
	g_object_set (G_OBJECT ( textlabel_left ), "text", "Suojaustaso: SININEN", NULL);
	g_object_set (G_OBJECT ( textlabel_left ), "valignment", 2, NULL);
	g_object_set (G_OBJECT ( textlabel_left ), "halignment", 0, NULL);
	g_object_set (G_OBJECT ( textlabel_left ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( textlabel_left ), "font-desc", "Sans, 14", NULL);
	
	double load[3];  
	char loadbuf[100];
	memset(loadbuf,0,100);
	if (getloadavg(load, 3) != -1)
	{  
		sprintf(loadbuf, "%f , %f , %f", load[0],load[1],load[2]);
	}
	
	textlabel_right = gst_element_factory_make ("textoverlay", NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "text", loadbuf, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "valignment", 2, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "halignment", 2, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( textlabel_right ), "font-desc", "Courier, 20", NULL);
	 
	timelabel = gst_element_factory_make ("timeoverlay", NULL);
	g_object_set (G_OBJECT ( timelabel ), "text", "Stream time:\n", NULL);
	g_object_set (G_OBJECT ( timelabel ), "valignment", 1, NULL);
	g_object_set (G_OBJECT ( timelabel ), "halignment", 2, NULL);
	g_object_set (G_OBJECT ( timelabel ), "shaded-background", TRUE, NULL);
	g_object_set (G_OBJECT ( timelabel ), "line-alignment", 0, NULL);
	g_object_set (G_OBJECT ( timelabel ), "font-desc", "Courier, 14", NULL);
	
	clocklabel = gst_element_factory_make ("clockoverlay", NULL);
	g_object_set (G_OBJECT ( clocklabel ), "text", "ID:Helsinki\n", NULL);
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
	g_object_set(G_OBJECT(sink), "clients", "0.0.0.0:5000,127.0.0.1:4999", NULL);
	
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
	gst_bin_add_many (GST_BIN (pipeline), source,capsfilter,textlabel_left,textlabel_right,timelabel,clocklabel,videoconverter, encoder,mux,payload, sink, NULL);

	if ( gst_element_link_many (source,capsfilter,textlabel_left,textlabel_right,timelabel,clocklabel,videoconverter, encoder,mux,payload,sink,NULL) != TRUE) {
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
