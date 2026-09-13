$fn=72;

// ESP32 CW KEYER V2 enclosure - revised front/rear panels
// TFT window is PARAMETRIC. Default 60 x 45 mm; verify against the exact visible glass area.
pcb_w=227.750;
pcb_d=132.860;
clear=8.0;
wall=3;
bottom=3;
outer_w=pcb_w+2*clear;   // 243.75 mm
outer_d=pcb_d+2*clear;   // 148.86 mm
case_h=108;              // increased for 2.8" TFT + controls
panel_t=3;

tft_win_w=60.0;
tft_win_h=45.0;
tft_x=outer_w/2;
tft_y=76;

mem_y=43;
ctrl_y=20;

module pcb_standoffs() {
    for(p=[[8.250,16.110],[222.750,15.860],[10.000,123.360],[218.000,123.110]]) {
        translate([clear+p[0],clear+p[1],bottom])
            difference() {
                cylinder(h=7,d=8);
                translate([0,0,-0.1]) cylinder(h=7.2,d=3.2);
            }
    }
}

module base() {
    union() {
        cube([outer_w,outer_d,bottom]);
        cube([wall,outer_d,case_h]);
        translate([outer_w-wall,0,0]) cube([wall,outer_d,case_h]);
        cube([outer_w,wall,bottom+6]);
        translate([0,outer_d-wall,0]) cube([outer_w,wall,bottom+6]);
        for(x=[6,outer_w-6]) {
            translate([x,5,bottom]) cylinder(h=case_h-6,d=8);
            translate([x,outer_d-5,bottom]) cylinder(h=case_h-6,d=8);
        }
        pcb_standoffs();
    }
}

module lid() {
    difference() {
        union() {
            cube([outer_w,outer_d,3]);
            translate([wall+0.5,wall+0.5,-3]) cube([2,outer_d-2*(wall+0.5),3]);
            translate([outer_w-wall-2.5,wall+0.5,-3]) cube([2,outer_d-2*(wall+0.5),3]);
        }
        for(x=[55:12:190]) translate([x,38,-0.1]) cube([6,72,3.2]);
    }
}

module engrave(txt,x,y,size=3.2) {
    translate([x,y,panel_t-0.7])
        linear_extrude(height=0.8)
            text(txt,size=size,halign="center",valign="center",font="Liberation Sans:style=Bold");
}

// Simple engraved headphone pictogram, independent of emoji/font support.
module headphone_icon(x,y,s=1) {
    translate([x,y,panel_t-0.7]) linear_extrude(height=0.8) scale([s,s]) {
        union() {
            difference() {
                circle(r=5.0);
                circle(r=3.6);
                translate([-6,-6]) square([12,6]);
            }
            translate([-5.1,-1.8]) square([1.8,4.5],center=true);
            translate([ 5.1,-1.8]) square([1.8,4.5],center=true);
        }
    }
}

module front_panel() {
    difference() {
        cube([outer_w,case_h,panel_t]);

        // TFT: only visible screen area through the panel
        translate([tft_x-tft_win_w/2,tft_y-tft_win_h/2,-0.1])
            cube([tft_win_w,tft_win_h,panel_t+0.2]);

        // MEM buttons directly below display
        for(x=[83,109,135,161]) translate([x,mem_y,-0.1]) cylinder(h=panel_t+0.2,d=7);

        // Bottom controls: MENU / VOLUME / HEADPHONE / PADDLE / STRAIGHT
        translate([42,ctrl_y,-0.1]) cylinder(h=panel_t+0.2,d=7.2);   // EC11 MENU
        translate([82,ctrl_y,-0.1]) cylinder(h=panel_t+0.2,d=7.2);   // 10K VOLUME
        translate([122,ctrl_y,-0.1]) cylinder(h=panel_t+0.2,d=6.5);  // headphone 3.5 mm
        translate([166,ctrl_y,-0.1]) cylinder(h=panel_t+0.2,d=10);   // paddle
        translate([211,ctrl_y,-0.1]) cylinder(h=panel_t+0.2,d=10);   // straight

        for(x=[6,outer_w-6]) for(z=[7,case_h-7])
            translate([x,z,-0.1]) cylinder(h=panel_t+0.2,d=3.2);

        // Engraved front labels
        engrave("CW_KEYER_V2",outer_w/2,102,4.0);
        engrave("M1",83,52,3.0); engrave("M2",109,52,3.0);
        engrave("M3",135,52,3.0); engrave("M4",161,52,3.0);
        engrave("MENU",42,8,3.0);
        engrave("VOLUME",82,8,2.8);
        headphone_icon(122,8,0.75);
        engrave("PADDLE",166,8,2.6);
        engrave("STRAIGHT",211,8,2.4);
    }
}

module rear_panel() {
    difference() {
        cube([outer_w,case_h,panel_t]);

        // Main rear connections
        translate([28,54,-0.1]) cylinder(h=panel_t+0.2,d=12.0);   // 12V input
        translate([58,54,-0.1]) cylinder(h=panel_t+0.2,d=6.5);    // power switch
        translate([98,54,-0.1]) cylinder(h=panel_t+0.2,d=16.2);   // CAT universal AD611
        translate([136,54,-0.1]) cylinder(h=panel_t+0.2,d=7.0);   // ICOM CI-V
        translate([168,54,-0.1]) cylinder(h=panel_t+0.2,d=10.0);  // KEY OUT
        translate([198,54,-0.1]) cylinder(h=panel_t+0.2,d=10.0);  // PTT OUT
        translate([220,49,-0.1]) cube([16,10,panel_t+0.2]);        // USB-C programming access

        for(x=[6,outer_w-6]) for(z=[7,case_h-7])
            translate([x,z,-0.1]) cylinder(h=panel_t+0.2,d=3.2);

        engrave("CW_KEYER_V2",outer_w/2,100,4.0);
        engrave("12V INPUT",28,37,2.7);
        engrave("POWER",58,37,2.6);
        engrave("CAT_UNIVERSEEL",98,36,2.3);
        engrave("ICOM CI-V",136,37,2.4);
        engrave("KEY",168,37,2.7);
        engrave("PTT",198,37,2.7);
        engrave("USB-C",228,35,2.5);
        engrave("PROGRAM",228,30,2.2);
    }
}

// Uncomment ONE line for interactive preview if desired:
// base();
// lid();
// front_panel();
// rear_panel();
