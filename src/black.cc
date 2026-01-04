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
#include <string.h>
#include <signal.h>
#include <filesystem>

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
std::string opt_commandline = ""; // command line arguments for logging

// fade function vars
struct timespec ts_fadestart;
struct timespec ts_fadeend;
struct timespec ts_currenttime;
struct timespec ts_elapsed;
double opt_fadein=0, opt_fadeout=0; //default fade 0s
double new_r=0, new_g=0, new_b=0; // rgb values during fade
double fadeprogress;
double elapsedtime;

// Retrieve current working directory and set log file path
std::string logName = "ft-black.log";   
std::filesystem::path cwd = std::filesystem::current_path();
std::filesystem::path logDir = cwd / "logs";
std::filesystem::path logPath = logDir / logName;

// create logger instance
Logger logger(logName);

int usage(const char *progname) {

    fprintf(stderr, "Black (c) 2016 Carl Gorringe (carl.gorringe.org)\n");
    fprintf(stderr, "Modified Version (c) 2025-2026 James Crowley (judgejc.net)\n");
    fprintf(stderr, "Usage: %s [options] [all]\n", progname);
    fprintf(stderr, "Options:\n"
        "\t-g <W>x<H>[+<X>+<Y>] : Output geometry. (default 64x64+0+0)\n"
        "\t-l <layer>     : Layer 0-15. (default 0)\n"
        "\t-t <timeout>   : Timeout exits after given seconds. (default now)\n"
        "\t-h <host>      : Flaschen-Taschen display hostname. (FT_DISPLAY)\n"
        "\t-b             : Black out with color (1,1,1)\n"
        "\t-c <RRGGBB>    : Fill with color as hex\n"
        "\t-I <fadein>    : Fade in demo over given seconds. (default 0s)\n"
        "\t-O <fadeout>   : Fade out demo over given seconds. (default 0s)\n"
        "\t all           : Clear ALL layers\n"
    );
    return 1;
}

int cmdLine(int argc, char *argv[]) {

    // command line options
    int opt;
    while ((opt = getopt(argc, argv, "?l:t:g:h:bc:I:O:")) != -1) {
        switch (opt) {
        case '?':  // help
            return usage(argv[0]);
            break;
        case 'g':  // geometry
            if (sscanf(optarg, "%dx%d%d%d", &opt_width, &opt_height, &opt_xoff, &opt_yoff) < 2) {
                logger.log(ERROR, "Invalid geometry '" + std::string(optarg) + "'");
                return usage(argv[0]);
            }
            break;
        case 'l':  // layer
            if (sscanf(optarg, "%d", &opt_layer) != 1 || opt_layer < 0 || opt_layer >= 16) {
                logger.log(ERROR, "Invalid layer '" + std::string(optarg) + "'");
                return usage(argv[0]);
            }
            break;
        case 't':  // timeout
            if (sscanf(optarg, "%lf", &opt_timeout) != 1 || opt_timeout < 0) {
                logger.log(ERROR, "Invalid timeout '" + std::string(optarg) + "'");
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
                logger.log(ERROR, "Color parse error for '" + std::string(optarg) + "'");
                return usage(argv[0]);
            }
            opt_fill = true;
            break;
        case 'I':  // fade in
            if (sscanf(optarg, "%lf", &opt_fadein) != 1 || opt_fadein < 0.0f) {
                logger.log(ERROR, "Invalid fade in '" + std::string(optarg) + "'");
                return usage(argv[0]);
            }
            break;
        case 'O':  // fade out
            if (sscanf(optarg, "%lf", &opt_fadeout) != 1 || opt_fadeout < 0.0f) {
                logger.log(ERROR, "Invalid fade out '" + std::string(optarg) + "'");
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

// Calculates difference between two timespecs for fade timing
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

// Enum to represent fade status
enum FadeStatus { START, FADEIN, FADEOUT, END };

void updateFadeProgress(FadeStatus status) {

    // Get current fade status
    switch (status) {
        case START:
            // get fade start time
            clock_gettime(CLOCK_MONOTONIC, &ts_fadestart);
            logger.log(DEBUG, "Fade in started at: " + 
                std::to_string((long long)ts_fadestart.tv_sec) + "." + 
                std::to_string(ts_fadestart.tv_nsec)
            );
            break;
        case FADEIN:
        case FADEOUT:                    
            // calculate fade progress based on elapsed time           
            clock_gettime(CLOCK_MONOTONIC, &ts_currenttime);
            ts_elapsed = timespec_diff(ts_currenttime, ts_fadestart);            
            elapsedtime = ((double)(ts_elapsed.tv_sec) + ((double)(ts_elapsed.tv_nsec) / 1000000000));        
            fadeprogress = elapsedtime / opt_fadein;

            if (status == FADEIN) {
                // calculate new rgb values based on fade progress
                new_r = opt_r * fadeprogress;
                new_g = opt_g * fadeprogress;
                new_b = opt_b * fadeprogress;

                // limit RGB max value to 255
                if (new_r > 255) { new_r = 255; };
                if (new_g > 255) { new_g = 255; };
                if (new_b > 255) { new_b = 255; };
            }
            else if (status == FADEOUT) {
                // calculate new rgb values based on fade progress
                new_r = opt_r * (1.0 - fadeprogress);
                new_g = opt_g * (1.0 - fadeprogress);
                new_b = opt_b * (1.0 - fadeprogress);

                // limit RGB min value to 0
                if (new_r < 0) { new_r = 0; };
                if (new_g < 0) { new_g = 0; };
                if (new_b < 0) { new_b = 0; };
            }

            // debug output
            logger.log(DEBUG, "Fade progress: " + 
                std::to_string(fadeprogress * 100) + "%, " +
                "current time: " + 
                std::to_string((long long)ts_currenttime.tv_sec) + "." + 
                std::to_string(ts_currenttime.tv_nsec) + ", " +
                "elapsed time: " + 
                std::to_string(elapsedtime) + "s, " +
                "original rgb: [" + std::to_string(opt_r) + ", " + 
                                 std::to_string(opt_g) + ", " + 
                                 std::to_string(opt_b) + "], " +
                "modified rgb: [" + std::to_string((int)new_r) + ", " + 
                                 std::to_string((int)new_g) + ", " + 
                                 std::to_string((int)new_b) + "]"
            );
            break;
        case END:
            // get fade end time
            clock_gettime(CLOCK_MONOTONIC, &ts_fadeend);
            logger.log(DEBUG, "Fade in ended at: " + 
                std::to_string((long long)ts_fadeend.tv_sec) + "." + 
                std::to_string(ts_fadeend.tv_nsec)
            );
            break;
    }
}

void argsToString() {

    // Construct command line argument string for debugging
    opt_commandline += (opt_hostname ? std::string("-h ") + opt_hostname + " " : "");
    opt_commandline += "-g " + std::to_string(opt_width) + "x" + std::to_string(opt_height) + 
               "+" + std::to_string(opt_xoff) + "+" + std::to_string(opt_yoff) + " ";
    opt_commandline += "-l " + std::to_string(opt_layer) + " ";
    opt_commandline += "-t " + std::to_string(opt_timeout) + " ";
    opt_commandline += (opt_black ? "-b " : "");
    if (opt_fill) {
        char colorstr[8];
        snprintf(colorstr, sizeof(colorstr), "%02x%02x%02x", opt_r, opt_g, opt_b);
        opt_commandline += std::string("-c ") + colorstr + " ";
    }
    opt_commandline += "-I " + std::to_string(opt_fadein) + " ";
    opt_commandline += "-O " + std::to_string(opt_fadeout) + " ";
    opt_commandline += (opt_all ? "all " : "");
}

int main(int argc, char *argv[]) {

    // Debug output
    logger.log(DEBUG, "Log file name: " + logName);
    logger.log(DEBUG, "Current working directory: " + cwd.string());
    logger.log(DEBUG, "Log file path: " + logPath.string());

    // log start of demo
    logger.log(INFO, "Starting ft-black demo");

    // parse command line
    if (int e = cmdLine(argc, argv)) { return e; }

    // log command line arguments
    argsToString();
    logger.log(DEBUG, "Command line arguments: " + opt_commandline);

    // Open socket and create our canvas.
    const int socket = OpenFlaschenTaschenSocket(opt_hostname);
    UDPFlaschenTaschen canvas(socket, opt_width, opt_height);
    logger.log(DEBUG, "Created new UDPFlaschenTaschen canvas: " + 
        std::to_string(opt_width) + "x" + std::to_string(opt_height) +
        " on host " + (opt_hostname ? std::string(opt_hostname) : "default"));

    // color, black, or clear
    if (opt_fill) {
        canvas.Fill(Color(opt_r, opt_g, opt_b));
        if (opt_all) {
            logger.log(INFO, "Filling all layers with color RGB(" + std::to_string(opt_r) + "," +
                std::to_string(opt_g) + "," + std::to_string(opt_b) + ")"
            );
        }
        else {
            logger.log(INFO, "Filling layer " + std::to_string(opt_layer) + " with color RGB(" +
                std::to_string(opt_r) + "," +
                std::to_string(opt_g) + "," +
                std::to_string(opt_b) + ")"
            );
        }
    }
    else if (opt_black) {
        canvas.Fill(Color(1, 1, 1));
        if (opt_all) {
            logger.log(INFO, "Filling all layers with black RGB(1,1,1)");
        }
        else {
            logger.log(INFO, "Filling layer " + std::to_string(opt_layer) + " with black RGB(1,1,1)");
        }
    }
    else {
        canvas.Clear();
        if (opt_all) {
            logger.log(INFO, "Clearing all layers");
        }
        else {
            logger.log(INFO, "Clearing layer " + std::to_string(opt_layer));
        }
    }

    if (opt_fadein > 0) {
        logger.log(INFO, "Applying fade in over " + std::to_string(opt_fadein) + " seconds");
    }

    if (opt_fadeout > 0) {
        logger.log(INFO, "Applying fade out over " + std::to_string(opt_fadeout) + " seconds");
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
            // handle fade in if specified
            if (opt_fadein > 0 && difftime(time(NULL), starttime) <= opt_fadein) {
              
              // fade in start
              updateFadeProgress(START);
              
              do {
                    // calculate new rgb values based on fade progress
                    updateFadeProgress(FADEIN);

                    // fill canvas with new rgb values
                    canvas.Fill(Color((int)new_r, (int)new_g, (int)new_b));
                    canvas.SetOffset(opt_xoff + DISPLAY_XOFF, opt_yoff + DISPLAY_YOFF, opt_layer);
                    canvas.Send();

                    // zzzzz.. for 100ms
                    usleep(100000);

              } while ( elapsedtime < opt_fadein );
              
              // fade in end
              updateFadeProgress(END);
            }
            else {
                // no fade in, just clear the layer
                canvas.SetOffset(opt_xoff + DISPLAY_XOFF, opt_yoff + DISPLAY_YOFF, opt_layer);
                canvas.Send();
            }

            // handle fade out if specified
            if (opt_fadeout > 0 && difftime(time(NULL), starttime) + opt_fadeout >= opt_timeout) {
                
                // fade out start
                updateFadeProgress(START);
                
                do {
                    // calculate new rgb values based on fade progress
                    updateFadeProgress(FADEOUT);

                    // fill canvas with new rgb values
                    canvas.Fill(Color((int)new_r, (int)new_g, (int)new_b));
                    canvas.SetOffset(opt_xoff + DISPLAY_XOFF, opt_yoff + DISPLAY_YOFF, opt_layer);
                    canvas.Send();

                    // zzzzz.. for 100ms
                    usleep(100000);

                } while ( elapsedtime < opt_fadeout );
                
                // fade out end
                updateFadeProgress(END);
            }
        }

        sleep(1);

    } while ( difftime(time(NULL), starttime) <= opt_timeout );

    // log end of demo
    logger.log(INFO, "Exiting ft-black demo");

    // clear canvas on exit
    canvas.Clear();
    canvas.Send();

    return 0;
}
