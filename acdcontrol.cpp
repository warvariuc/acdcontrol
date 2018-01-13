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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#define HID_MAX_USAGES			1024
#define HID_MAX_APPLICATIONS		16

#define VERSION "0.3"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <sys/signal.h>
#include <getopt.h>
#include <linux/hiddev.h>

#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <list>

using namespace std;

const int GET = 0;
const int SET = 1;
const int DETECT = 2;
const int SETREL = 3;

// Supported vendors
const int APPLE                           = 0x05ac;
const int SAMSUNG                         = 0x0419;

const int BRIGHTNESS_CONTROL              = 16;
const int USAGE_CODE                      = 0x820010;

const int STUDIO_DISPLAY_15               = 0x9215;
const int STUDIO_DISPLAY_17               = 0x9217;
const int CINEMA_DISPLAY_23_OLD           = 0x9218;
const int CINEMA_DISPLAY_23_NEW           = 0x9218;
const int CINEMA_DISPLAY_20_OLD           = 0x9219;
const int CINEMA_DISPLAY_20_NEW           = 0x9222;
const int CINEMA_DISPLAY_24               = 0x921e;
const int CINEMA_DISPLAY_27               = 0x9226;
const int CINEMA_DISPLAY_27_2013          = 0x9227;
const int CINEMA_DISPLAY_30               = 0x9232;
const int CINEMA_LED_DISPLAY_24           = 0x9236;

const int S1                              = 0x8002;

// Forward Declarations
void init_device_database();
void dump_supported();

// Helpful declarations
typedef unsigned Vendor;
typedef unsigned Product;

struct DeviceId {
  Product product;
  Vendor vendor;
  string description;
  
  DeviceId ( Vendor vendor_, Product product_, string description_ )
    : product( product_ )
    , vendor( vendor_ )
    , description( description_ ) { }

  bool operator < ( const DeviceId& other ) const {
    return (vendor < other.vendor) || 
      (vendor == other.vendor && product < other.product );
  }
};

typedef set< DeviceId > SupportedDevices;
SupportedDevices supportedDevices;

typedef map< Vendor, string > SupportedVendors;
SupportedVendors supportedVendors;

typedef pair< Vendor, string> VendorDesc;


/** @return whether the string *seems* to be a number */
bool number( const char* str ){
  if (!str ) return false;
  
  return ((*str >= '0') && (*str <= '9')) || (*str == '+') || (*str == '-');
}

/** @return true if the device is in our database */
bool is_supported ( const hiddev_devinfo& device_info ) {
  Product product = device_info.product & 0xFFFF;
  Vendor vendor = device_info.vendor & 0xFFFF;

  return supportedDevices.find( DeviceId( vendor, product, "" )) != 
    supportedDevices.end();
}

/**
 * @param v query vendor
 * @param p query product
 * @return description of the device with given vendor and product 
 */
string description ( Vendor v, Product p ) {
  SupportedDevices::iterator it = 
    supportedDevices.find( DeviceId( v, p, "" ));

  if ( it != supportedDevices.end() )
    return it->description;

  return "";
}

/** @param v vendor to query
 * @return true if the venodr is in the database 
 */
bool known_vendor ( Vendor v ) {
  v &= 0xFFFF;
  return supportedVendors.find( v ) != supportedVendors.end();
}

/** Checks whether the HID device is a usb monitor 
 * @param device_info HID device info
 * @param fd file to read applications from
 */
bool is_usb_monitor ( const hiddev_devinfo& device_info, int fd ) {
  
  /* Now that we have the number of applications, we can retrieve them */
  /* using the HIDIOCAPPLICATION ioctl() call */
  /* applications are indexed from 0..{num_applications-1} */
  for ( int appl_num = 0; appl_num < device_info.num_applications; 
        ++appl_num ) {
    int application = ioctl( fd, HIDIOCAPPLICATION, appl_num );
    /* The magic values come from various usage table specs */
    if ( ((application >> 16) & 0xFF) == 0x80 ) {
      return true;
    }
  }
  return false;
}

/** Pretty-prints the given device information
 * @param o output stream to print to
 * @param device_info HID device info
 */
void format_device( ostream& o, const hiddev_devinfo& device_info ) {
  Vendor  v = device_info.vendor & 0xFFFF;
  Product p = device_info.product & 0xFFFF;
  o << "Vendor=" << showbase << setw( 6 ) << hex << v;
  if ( known_vendor( v ) )
    o << " (" << supportedVendors[ v ] << ")";
  
  o << ", Product=" << showbase << setw( 6 ) << hex << p ;

  if ( is_supported( device_info )) 
    o << "[" << description( v, p ) << "]";

  o << endl;
}


/** Debug routine for dumping device usage 
 * @param usage_ref usage description 
 */
void dump_usage ( hiddev_usage_ref& usage_ref ) {
  printf ("  report_type   =%d\n", usage_ref.report_type );
  printf ("  report_id     =%d\n", usage_ref.report_id );
  printf ("  field_index   =%d\n", usage_ref.field_index );
  printf ("  usage_index   =%d\n", usage_ref.usage_index );
  printf ("  usage_code    =%d\n", usage_ref.usage_code );
}

/** Prints help for the program.
 * @param programName this program name
 */
void help( const char *programName ) {
  printf( "acdcontrol " VERSION "\n");

  printf( "USAGE: %s [--silent|-s] [--brief|-b] [--help|-h] [--about|-a] "
          "[--detect|-d] [--list-all |-l] <hid device(s)> [<brightness>]\n\n"
          "Parameters:\n"
          "  --silent,-s\n"
          "         Suppress non-functional program output\n"
          "  --brief,-b\n"
          "         Print brightness value only when in query mode,\n"
          "         otherwise ignored.\n"
          "  --detect, -d\n"
          "         Perform detection only\n"
          "  --list-all, -l\n"
          "         List supported devices and exit\n"
          "  --help,-h\n"
          "         Show short help message and quit.\n"
          "  --about,-a\n"
          "         Show information about the program, some credits and thanks.\n"
          "  hid device\n"
          "         device that represents your Apple Cinema display.\n"
          "         It shoud normally be one of /dev/usb/hiddevX. or /dev/hiddevX\n"
          "      Note\n"
          "         You must have write permissions to this device.\n"
          "      Note\n"
          "         It should be safe to run the program on other device than\n"
          "         Apple Cinema/Studio Display as the program checks whether\n"
          "         the device is Apple display and warns about it.\n"
          "  brightness\n"
          "         When this option is specified, the operation is to set brightness,\n"
          "         otherwise, current brightness is retreived. If brightness starts\n"
          "         with '+' or '-', the current brightness is increased or decreased\n"
          "         by that value.\n"
          "      Note\n"
          "         Preceed brightness decrement by '--'.\n"
          "      See also: --brief option.\n"

          "\n\nEXAMPLES:\n\n"
          "In the following examples I assume that your HID device is /dev/hiddev0. You may \n"
          "have it as /dev/hiddevX or /dev/usb/hiddevX.\n"
          "\n"
          "  acdcontrol\n"
          "  acdcontrol --help\n"
          "      Show long help message.\n"
          "\n"
          "  acdcontrol --detect /dev/hiddev*\n"
          "      Perform detection, which HID device is actually your display to be controlled.\n"
          "\n"
          "  acdcontrol /dev/hiddev0\n"
          "      Read current brightness parameter\n"
          "\n"
          "  acdcontrol /dev/hiddev0 160\n"
          "      Set brightness to 160. Note, that brightness setting depends on your model. \n"
          "      Generally, this parameter may get values in the range [0-255].\n"
          "\n"
          "  acdcontrol /dev/hiddev0 +10\n"
          "      Increment current brightness by 10.\n"
          "\n"
          "  acdcontrol /dev/hiddev0 -- -10\n"
          "      Decrement current brightness by 10. Please,note '--'!\n"
          ,

      
          programName );
}

/** Prints brief notice about the program */
void notice() {
  printf( "Apple Cinema and Studio Display Control Program. Please, use --about switch to learn more\n" );
}

/** Prints detailed info about this program */
void about() {
  printf( "acdcontrol " VERSION "\n");

  printf( "Written by Pavel Gurevich.\n\n"
          "This program is a free software and is distributed under GPL2\n\n"
          "CREDITS\n\n"
          "This program would never possibly come out without valuable help of Andre Beckedorf."
          " Andre wrote a similar software for Windows. Please, visit http://metaexception.de/."
          " Andre would like to thank to people that helped him to write his software and I'd "
          "like to join him as they helped me that way :)\n\n"
          "    * Dmitri Kitaynik (20\" Display)\n"
          "    * Mark Wagner (15\" and 17\" Displays)\n" 
          "    * Veit Wahlich (30\" Display)\n" 
          "    * Charles Lepple (24\" Display)\n" 
          "    * Arne Zellentin (relative brightness change)\n\n"
          " Please, visit http://acdcontrol.sf.net for updates\n"
          "NOTE: You can suppress a startup message with --silent (-s) option\n"
          );
}

////////////////////////////////////////////////////////////////////////////////
//                      _
//                     (_)
//  _ __ ___     __ _   _    _ __
// | '_ ` _ \   / _` | | |  | '_ \
// | | | | | | | (_| | | |  | | | |
// |_| |_| |_|  \__,_| |_|  |_| |_|
//
////////////////////////////////////////////////////////////////////////////////
int main (int argc, char **argv) {
  int fd = -1;
  int rd, i;
  int alv, yalv;
  struct hiddev_devinfo device_info;
  struct hiddev_report_info rep_info;
  struct hiddev_field_info field_info;
  struct hiddev_usage_ref usage_ref;
  struct hiddev_event ev[64];
  fd_set fdset;
  int report_type;
  int appl;
  int version;
  int brightness = 0;
  int amount = 0;
  int mode = GET;
  int open_mode = O_RDONLY;
  
  /* Behavior options */
  bool brief  = false;
  bool silent = false;
  bool force = false;

  bool first_device=true;
    
  int c;
  int digit_optind = 0;

  init_device_database();
  
  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
      {"about", 0, 0, 'a'},
      {"brief", 0, 0, 'b'},
      {"help", 0, 0, 'h'},
      {"silent", 0, 0, 's'},
      {"force", 0, 0, 'f'},
      {"detect", 0, 0, 'd'},
      {"list-all", 0, 0, 'l'},
      {0, 0, 0, 0}
    };
      
    c = getopt_long (argc, argv, "abhsdl",
                     long_options, &option_index);
    if (c == -1)
      break;
      
    switch (c) {
    case 'a':
      about();
      exit( 0 );
        
    case 'b':
      brief=true;
      break;
        
    case 'h':
      help( argv[0] );
      exit( 0 );
      break;
        
    case 's':
      silent=true;
      break;
      
    case 'f':
      force=true;
      break;

    case 'd':
      mode=DETECT;
      break;

    case 'l':
      dump_supported();
      exit( 0 );
        
    default:
      fprintf (stderr,"Unknown option '%c'\n", c);
      help( argv[0] );
      exit( 2 );
    }
  }

  typedef list< const char* > FileList;
  FileList files;
  
  for ( int param = optind; param < argc; ++param ) {
    if ( mode != DETECT && number ( argv[ param ] ) ) {
      if ( argv[ param ][0] == '+' || argv[ param ][0] == '-' ) {
        mode = SETREL;
        amount = atoi ( argv[ param ] );
      } else {
        mode = SET;
        brightness = atoi ( argv[ param ] );
      }
      continue;
    }

    files.push_back( argv[ param ] );
  }

  if ( files.empty() ) {
    help( argv[0] );
    exit( 1 );
  }

  if ( mode == SET || mode == SETREL ) {
    open_mode = O_RDWR;
  }

  if ( !silent )
    notice();

  for ( FileList::iterator it = files.begin(); it != files.end();
        ++it ) {
    if (( fd = open( *it, open_mode )) < 0) {
      perror(*it);
      continue;
    }
    
    /* ioctl() accesses the underlying driver */
    ioctl(fd, HIDIOCGVERSION, &version);
    /* the HIDIOCGVERSION ioctl() returns a packed 32 field (aka integer) */
    /* so we unpack it and display it */
    if ( ! silent && first_device )
      printf("hiddev driver version is %d.%d.%d\n",
             version >> 16, (version >> 8) & 0xff, version & 0xff);
    
    /* suck out some device information */
    ioctl(fd, HIDIOCGDEVINFO, &device_info);
    
    if ( mode == DETECT ) {
      if ( is_usb_monitor( device_info, fd ) ) {
        cout << *it << ": USB Monitor - "
             << (is_supported( device_info ) ? "SUPPORTED": "UNSUPPORTED")
             << ".\t";
        format_device( cout, device_info );
      }
      continue;
    }

    if ( !is_supported ( device_info ) ){
      cerr << "Device unsupported:";
      format_device(cerr, device_info);
      if ( !force )
        exit ( 2 );
    }
    
    
    if (! is_usb_monitor( device_info, fd )) {
      cerr << *it << ": This device is NOT USB monitor!" << endl;
      continue;
    }
    
    /* Initialise the internal report structures */
    if (ioctl(fd, HIDIOCINITREPORT,0) < 0) {
      cerr << "FATAL: Failed to initialize internal report structures"
           << endl;
      exit(1);
    }
    
    usage_ref.report_type = HID_REPORT_TYPE_FEATURE;
    usage_ref.report_id = BRIGHTNESS_CONTROL;
    usage_ref.field_index = 0;
    usage_ref.usage_index = 0;
    usage_ref.usage_code = USAGE_CODE;
    usage_ref.value = brightness;
    //  dump_usage ( usage_ref );
    
    rep_info.report_type = HID_REPORT_TYPE_FEATURE;
    rep_info.report_id = BRIGHTNESS_CONTROL;
    rep_info.num_fields = 1;
    
    if ( mode == SET ) {
      if ( ioctl(fd, HIDIOCSUSAGE, &usage_ref) < 0 ) {
        perror ("Usage failed!");
        exit ( 2 );
      }
      if ( ioctl(fd, HIDIOCSREPORT, &rep_info) < 0 ) {
        perror ("Report failed!");
        exit ( 3 );
      }
    } else {
      if ( ioctl(fd, HIDIOCGUSAGE, &usage_ref) < 0 ) {
        perror ("Usage failed!");
        exit ( 2 );
      }
      if ( ioctl(fd, HIDIOCGREPORT, &rep_info) < 0 ) {
        perror ("Report failed!");
        exit ( 3 );
      }
      if ( mode == SETREL ) {
        brightness = usage_ref.value + amount;
        brightness = max( 0, brightness);
        brightness = min( 750, brightness);
        usage_ref.value = brightness;
        
        /* set calculated brightness */
        if ( ioctl(fd, HIDIOCSUSAGE, &usage_ref) < 0 ) {
          perror ("Usage failed!");
          exit ( 2 );
        }
        if ( ioctl(fd, HIDIOCSREPORT, &rep_info) < 0 ) {
          perror ("Report failed!");
          exit ( 3 );
        }
        /* read brightness back from device */
        if ( ioctl(fd, HIDIOCGUSAGE, &usage_ref) < 0 ) {
          perror ("Usage failed!");
          exit ( 2 );
        }
        if ( ioctl(fd, HIDIOCGREPORT, &rep_info) < 0 ) {
          perror ("Report failed!");
          exit ( 3 );
        }
      }
      if ( !brief )
        cout << *it << ": BRIGHTNESS=";
      cout << usage_ref.value << endl;
    }

    close(fd);
    first_device=false;
  }
}



void init_device_database() {
  supportedVendors.insert( VendorDesc( SAMSUNG, "Samsung Electronics" ) );
  supportedVendors.insert( VendorDesc( APPLE, "Apple" ) );

  supportedDevices.insert( DeviceId( APPLE, STUDIO_DISPLAY_15,
                                     "Apple Studio Display 15\"" ));
  supportedDevices.insert( DeviceId( APPLE, STUDIO_DISPLAY_17,
                                     "Apple Studio Display 17\"" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_20_NEW,
                                      "Apple Cinema Display 20\" (new)" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_20_OLD,
                                     "Apple Cinema Display 20\" (old)" ));
//   supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_23_NEW,
//                                      "Apple Cinema Display 23\" (new)" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_23_OLD,
                                     "Apple Cinema Display 23\" (old)" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_24,
                                     "Apple Cinema Display 24\"" ));

  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_27,
                                     "Apple Cinema HD Display 27\"" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_27_2013,
                                     "Apple Cinema HD Display 27\"" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_DISPLAY_30,
                                     "Apple Cinema HD Display 30\"" ));
  supportedDevices.insert( DeviceId( APPLE, CINEMA_LED_DISPLAY_24,
                                     "Apple LED Cinema Display 24\"" ));

  supportedDevices.insert( DeviceId( SAMSUNG, S1,
                                     "Samsung SyncMaster 757NF" ));
}

void dump_supported () {
  for ( SupportedDevices::iterator it = supportedDevices.begin();
        it != supportedDevices.end(); ++ it )
    cout << "Vendor=" << setw( 6 ) << hex << showbase << it->vendor
         << " (" << supportedVendors[ it->vendor ] << "), "
         << "Product=" << it->product << " [" 
         << it->description << "]" << endl;
}
