// -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
//
// black
// Copyright (c) 2016 Carl Gorringe (carl.gorringe.org)
// https://github.com/cgorringe/ft-demos
// 5/2/2016
//
// Modified Version 2025 James Crowley (judgejc.net)
// https://github.com/judgejc/ft-demos
//
// 30/12/2025 - Update help message to display new defaults for output geometry.
// 31/12/2025 - Implemented a fade routine which can be optionally enabled on a
// specified layer at the start or end of the demo to ease scene transitions.
// 01/01/2026 - Included utility extensions for Flaschen Taschen demos to enable
// simple logging functionality (ft-utils.cc/h).
//
// Clears the Flaschen Taschen canvas.
//
// How to run:
//
// To see command line options:
//  ./lines -?
//
// By default, connects to the installation at Noisebridge. If using a
// different display (e.g. a local terminal display)
// pass the hostname as parameter:
//
//  ./black -h localhost
//
// or set the environment variable FT_DISPLAY to not worry about it
//
//  export FT_DISPLAY=localhost
//  ./black
//
// --------------------------------------------------------------------------------
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>
//

#include "udp-flaschen-taschen.h"
#include "ft-logger.h"

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string>
#include <string.h>
#include <signal.h>

#include "config.h"
#define Z_LAYER 0      // (0-15) 0=background

// ------------------------------------------------------------------------------------------
// Command Line Options

// option vars
const char *opt_hostname = NULL;
int opt_layer  = Z_LAYER;
double opt_timeout = 0;  // timeout now
int opt_width  = DISPLAY_WIDTH;
int opt_height = DISPLAY_HEIGHT;
int opt_xoff=0, opt_yoff=0;
bool opt_black = false;
bool opt_all = false;
bool opt_fill = false;
int opt_r=0, opt_g=0, opt_b=0;

// fade function vars
struct timespec ts_fadestart;
struct timespec ts_fadeend;
struct timespec ts_currenttime;
struct timespec ts_elapsed;
double opt_fadein=0, opt_fadeout=0; //default fade 0s
double new_r=0, new_g=0, new_b=0;
double fadeprogress;
double elapsedtime;

// create logger instance
Logger logger("logs/ft-black.log");

int usage(const char *progname) {

    fprintf(stderr, "Black (c) 2016 Carl Gorringe (carl.gorringe.org)\n");
    fprintf(stderr, "Modified Version 2025 James Crowley (judgejc.net)\n");
    fprintf(stderr, "Usage: %s [options] [all]\n", progname);
    fprintf(stderr, "Options:\n"
        "\t-g <W>x<H>[+<X>+<Y>] : Output geometry. (default 64x64+0+0)\n"
        "\t-l <layer>     : Layer 0-15. (default 0)\n"
        "\t-t <timeout>   : Timeout exits after given seconds. (default now)\n"
        "\t-h <host>      : Flaschen-Taschen display hostname. (FT_DISPLAY)\n"
        "\t-b             : Black out with color (1,1,1)\n"
        "\t-c <RRGGBB>    : Fill with color as hex\n"
        "\t-x <fadein>    : Fade in demo over given seconds. (default 0s)\n"
        "\t-y <fadeout>   : Fade out demo over given seconds. (default 0s)\n"
        "\t all           : Clear ALL layers\n"
    );
    return 1;
}

int cmdLine(int argc, char *argv[]) {

    // command line options
    int opt;
    while ((opt = getopt(argc, argv, "?l:t:g:h:bc:x:y:")) != -1) {
        switch (opt) {
        case '?':  // help
            return usage(argv[0]);
            break;
        case 'g':  // geometry
            if (sscanf(optarg, "%dx%d%d%d", &opt_width, &opt_height, &opt_xoff, &opt_yoff) < 2) {
                fprintf(stderr, "Invalid size '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'l':  // layer
            if (sscanf(optarg, "%d", &opt_layer) != 1 || opt_layer < 0 || opt_layer >= 16) {
                fprintf(stderr, "Invalid layer '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 't':  // timeout
            if (sscanf(optarg, "%lf", &opt_timeout) != 1 || opt_timeout < 0) {
                fprintf(stderr, "Invalid timeout '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'h':  // hostname
            opt_hostname = strdup(optarg); // leaking. Ignore.
            break;
        case 'b':  // black out
            opt_black = true;
            break;
        case 'c':
            if (sscanf(optarg, "%02x%02x%02x", &opt_r, &opt_g, &opt_b) != 3) {
                fprintf(stderr, "Color parse error\n");
                return usage(argv[0]);
            }
            opt_fill = true;
            break;
        case 'x':  // fade in
            if (sscanf(optarg, "%lf", &opt_fadein) != 1 || opt_fadein < 0.0f) {
                fprintf(stderr, "Invalid fade in '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        case 'y':  // fade out
            if (sscanf(optarg, "%lf", &opt_fadeout) != 1 || opt_fadeout < 0.0f) {
                fprintf(stderr, "Invalid fade out '%s'\n", optarg);
                return usage(argv[0]);
            }
            break;
        default:
            usage(argv[0]);
        }
    }

    // retrieve arg text
    const char *text = argv[optind];
    if (text && strncmp(text, "all", 3) == 0) {
        opt_all = true;
    }

    return 0;
}

// ------------------------------------------------------------------------------------------

// Calculates difference between two timespecs
struct timespec timespec_diff(struct timespec currenttime, struct timespec starttime) {
    
    // Declare time constants
    constexpr long max_msec = 1000;
    constexpr long max_usec = 1000*max_msec;
    constexpr long max_nsec = 1000*max_usec;
    
    // Calculate timespec difference
    struct timespec result;    
    result.tv_sec  = currenttime.tv_sec  - starttime.tv_sec;
    result.tv_nsec = currenttime.tv_nsec - starttime.tv_nsec;

    // Fix negative nanoseconds result
    if (result.tv_nsec < 0) {
        --result.tv_sec;
        result.tv_nsec += max_nsec;
    }
    return result;
}

int main(int argc, char *argv[]) {

    // parse command line
    if (int e = cmdLine(argc, argv)) { return e; }

    logger.log(INFO, "Starting ft-black demo");

    // Open socket and create our canvas.
    const int socket = OpenFlaschenTaschenSocket(opt_hostname);
    UDPFlaschenTaschen canvas(socket, opt_width, opt_height);
    logger.log(DEBUG, "Created new UDPFlaschenTaschen canvas: " + 
        std::to_string(opt_width) + "x" + std::to_string(opt_height) +
        " on host " + (opt_hostname ? std::string(opt_hostname) : "default"));

    // color, black, or clear
    if (opt_fill) {
        canvas.Fill(Color(opt_r, opt_g, opt_b));
        logger.log(INFO, "Filling layer " + std::to_string(opt_layer) + 
            " with color RGB(" + std::to_string(opt_r) + "," +
            std::to_string(opt_g) + "," + std::to_string(opt_b) + ")"
        );
    }
    else if (opt_black) {
        canvas.Fill(Color(1, 1, 1));
        logger.log(INFO, "Filling layer " + std::to_string(opt_layer) + 
            " with black RGB(1,1,1)"
        );
    }
    else {
        canvas.Clear();
        logger.log(INFO, "Clearing layer " + std::to_string(opt_layer));
    }

    if (opt_all) {
        printf("clear all layers\n");
        logger.log(INFO, "Clearing all layers");
    }
    else {
        printf("clear layer %d\n", opt_layer);
        logger.log(INFO, "Clearing layer " + std::to_string(opt_layer));
    }

    time_t starttime = time(NULL);
    do {
        if (opt_all) {
            // clear ALL layers
            for (int i=0; i <= 15; i++) {
                canvas.SetOffset(opt_xoff + DISPLAY_XOFF, opt_yoff + DISPLAY_YOFF, i);
                canvas.Send();
            }
        }
        else {
            // clear single layer
            canvas.SetOffset(opt_xoff + DISPLAY_XOFF, opt_yoff + DISPLAY_YOFF, opt_layer);
            canvas.Send();
        }

        sleep(1);

    } while ( difftime(time(NULL), starttime) <= opt_timeout );

    logger.log(INFO, "Exiting ft-black demo");
    return 0;
}
