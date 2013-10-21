/***************************************************************************
                             main.c  -  description
                           -------------------
    begin                : Mon Oct 21 09:41:54 CEST 2013
    copyright            : (C) 2013 by Brandon Warhurst
    email                : roboknight AT gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

/*
 TODO:
		- Ability to find device by PID/VID, product name or serial

 TODO nice-to-have:
    - Out-of-the-box compatibility with FTDI's eeprom tool configuration files
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <confuse.h>
#include <libusb-1.0/libusb.h>
#include <libftdi1/ftdi.h>
#include <getopt.h>
#include <ctype.h>

/**
 * @brief Convert driver options strings to a value
 *
 * \param str pointer to driver option string to convert
 *
 * Function will return the value of the option string.
 * This is used to determine the correct values to set for
 * drivers.
 * 
 **/
static int str_to_drvr(char *str) {
	const int max_options = 4;
	int cmp_len=0,i;
	const char* options[] = { "D2XX", "VCP", "RS485" };
	
	for(i = 0; i < max_options; i++) {
		cmp_len = (strlen(str) >= strlen(options[i])) ? strlen(options[i]) : strlen(str);
		if(!strncmp(str,options[i],cmp_len)) break;
	}

	return i;
}

/**
 * @brief Convert CBUS options strings to an index
 *
 * \param str pointer to CBUS option string to index
 * \param max_allowed last CBUS option to allow in returned index
 *
 * Function will return 0 if no options are found.  A message
 * will be sent to the terminal.
 **/
static int str_to_cbus(char *str, int max_allowed)
{
    #define MAX_OPTION 14
    const char* options[MAX_OPTION] = {
     "TXDEN", "PWREN", "RXLED", "TXLED", "TXRXLED", "SLEEP",
     "CLK48", "CLK24", "CLK12", "CLK6",
     "IO_MODE", "BITBANG_WR", "BITBANG_RD", "SPECIAL"};
    int i;
    max_allowed += 1;
    if (max_allowed > MAX_OPTION) max_allowed = MAX_OPTION;
    for (i=0; i<max_allowed; i++) {
        if (!(strcmp(options[i], str))) {
            return i;
        }
    }
    printf("WARNING: Invalid cbus option '%s'\n", str);
    return 0;
}

/**
 * @brief Set eeprom value
 *
 * \param ftdi pointer to ftdi_context
 * \param value_name Enum of the value to set
 * \param value Value to set
 *
 * Function will abort the program on error
 **/
static void eeprom_set_value(struct ftdi_context *ftdi, enum ftdi_eeprom_value value_name, int value)
{
    if (ftdi_set_eeprom_value(ftdi, value_name, value) < 0)
    {
        printf("Unable to set eeprom value %d: %s. Aborting\n", value_name, ftdi_get_error_string(ftdi));
        exit (-1);
    }
}

/**
 * @brief Get eeprom value
 *
 * \param ftdi pointer to ftdi_context
 * \param value_name Enum of the value to get
 * \param value Value to get
 *
 * Function will abort the program on error
 **/
static void eeprom_get_value(struct ftdi_context *ftdi, enum ftdi_eeprom_value value_name, int *value)
{
    if (ftdi_get_eeprom_value(ftdi, value_name, value) < 0)
    {
        printf("Unable to get eeprom value %d: %s. Aborting\n", value_name, ftdi_get_error_string(ftdi));
        exit (-1);
    }
}

/**
 * @brief Detect eeprom type
 *
 * \param ftdi pointer to ftdi_context
 * \param eeprom_type eeprom type to set if failure occurs
 *
 * Function will return the eeprom type detected or
 * the eeprom_type if detection failed.
 **/
static int detect_eeprom(struct ftdi_context *ftdi, int eeprom_type) {
	int i, f;
	
    f = ftdi_erase_eeprom(ftdi); /* needed to determine EEPROM chip type */
    if (ftdi_get_eeprom_value(ftdi, CHIP_TYPE, &i) <0)
    {
        fprintf(stderr, "ftdi_get_eeprom_value: %d (%s)\n",
                f, ftdi_get_error_string(ftdi));
        fprintf(stderr, "using configuration value = 0x%02x\n",eeprom_type);
        i = eeprom_type;
    } else {
		if (i == -1)
			fprintf(stderr, "No EEPROM\n");
		else if (i == 0)
			fprintf(stderr, "Internal EEPROM\n");
		else
			fprintf(stderr, "Found 93x%02x\n", i);
		if(i != eeprom_type && eeprom_type != 0) {
			printf("Unmatched EEPROM type 0x%02x. Using configuration value = 0x%02x\n",i, eeprom_type);
			i = eeprom_type;
		}
	}

	return i;
}

/**
 * @brief locate an ftdi device based on vid/pid values
 *
 * \param ftdi pointer to ftdi_context
 * \param primary_vid vid to try first
 * \param primary_pid pid to try first
 * \param fallback_vid vid to try if primary fails
 * \param fallback_pid pid to try if primary fails
 *
 * Function will attempt to locate the device with
 * primary_vid/primary_pid and fallback to fallback_vid/fallback_pid
 * if primary_vid/primary_pid can't be found.  Returns the
 * status of the ftdi_usb_open routine.
 **/
static int locate_ftdi_device(struct ftdi_context *ftdi, int primary_vid, int primary_pid, int fallback_vid, int fallback_pid)
{
	int i;

	i = ftdi_usb_open(ftdi, primary_vid, primary_pid);

	if(i!=0) {
		printf("Unable to find FTDI devices under given vendor/product id: 0x%X/0x%X\n", primary_vid, primary_pid);
		printf("Error code: %d (%s)\n", i, ftdi_get_error_string(ftdi));
		if(primary_vid != fallback_vid || primary_pid != fallback_pid) {
			printf("Retrying with fallback vid=%#04x, pid=%#04x.\n", fallback_vid, fallback_pid);

			i = ftdi_usb_open(ftdi, fallback_vid, fallback_pid);
			if (i != 0)
			{
				printf("Error: %s\n", ftdi->error_str);
			} else {
				printf("Alternate device (%04x,%04x) located.\n",fallback_vid,fallback_pid);
			}
		}
	} else {
		printf("Device (%04x,%04x) located.\n",primary_vid,primary_pid);
	}

	return i;
}

/**
 * @brief Display usage information
 *
 * \param prog_name pointer command line program name
 *         usually argv[0].
 *
 * Function will provide a short summary of program usage
 * to the command line.
 **/
static void usage(char *prog_name)
{
	printf("%s <command> [options]\n",prog_name);
	printf("commands (must choose one):\n");
	printf("-h\t\t\tthis help.\n");
	printf("-e\t\t\terase configuration eeprom.\n");
	printf("-f <config filename>\tprogram configuration eeprom using <config filename>.\n");
	printf("-r <config binary>\tread configuration eeprom and write it to <config binary>.\n");
	printf("-s\t\t\tscan for default FTDI devices.\n");
	printf("options:\n");
	printf("-o <filename>\t\twrite binary configuration to <filename> after read command.\n");
	printf("-d\t\t\tread and decode eeprom.\n");
	printf("-D\t\t\tdisplay hexdump of eeprom during decoding.\n");
	printf("-p <pid>\t\tuse pid <pid> for operation.\n");
	printf("-v <vid>\t\tuse vid <vid> for operation.\n");
	printf("NOTE 1: FTDI default vid is 0x403 and default pid is 0x6001\n");
	printf("      All other vid and pid values should be specified in the configuration file\n");
	printf("      or on the command line with -v and -p.\n");
	printf("NOTE 2: -o option is equivalent to 'filename' configuration file parameter, but for reading.\n");
	exit(-1);
}

/**
 * @brief Read and decode EEPROM information
 *
 * \param ftdi pointer to ftdi_context
 * \param debug value indicating whether more output is wanted.
 *         0 = standard output
 *         1 = debug output
 *
 * Function will provide the decoded EEPROM information
 * to the command line.  The debug output is the byte
 * buffer, displayed as a hex dump.
 **/
int read_decode_eeprom(struct ftdi_context *ftdi, int debug)
{
	int i, j, f;
	int value;
	int size;
	unsigned char buf[256];

	ftdi_get_eeprom_value(ftdi, CHIP_SIZE, &value);
	if (value <0)
	{
		fprintf(stderr, "No EEPROM found or EEPROM empty\n");
		return -1;
	}
	fprintf(stderr, "Chip type %d ftdi_eeprom_size: %d\n", ftdi->type, value);
	if (ftdi->type == TYPE_R)
		size = 0xa0;
	else
		size = value;

	if(debug > 0) {
		ftdi_get_eeprom_buf(ftdi, buf, size);
		for (i=0; i < size; i += 16)
		{
			fprintf(stdout,"0x%03x:", i);
					
			for (j = 0; j< 8; j++)
				fprintf(stdout," %02x", buf[i+j]);
			fprintf(stdout," ");
			for (; j< 16; j++)
				fprintf(stdout," %02x", buf[i+j]);
			fprintf(stdout," ");
			for (j = 0; j< 8; j++)
				fprintf(stdout,"%c", isprint(buf[i+j])?buf[i+j]:'.');
			fprintf(stdout," ");
			for (; j< 16; j++)
				fprintf(stdout,"%c", isprint(buf[i+j])?buf[i+j]:'.');
			fprintf(stdout,"\n");
		}
	}

	f = ftdi_eeprom_decode(ftdi, 1);
	if (f < 0)
	{
		fprintf(stderr, "ftdi_eeprom_decode: %d (%s)\n",
				f, ftdi_get_error_string(ftdi));
		return -1;
	}
	return 0;
}

#define QUIT return_code = 1; goto cleanup;

int main(int argc, char *argv[])
{
    /*
    configuration options
    */
    cfg_opt_t opts[] =
    {
        CFG_INT("target_vendor_id", 0x403, 0),
        CFG_INT("target_product_id", 0x6001, 0),
        CFG_INT("vendor_id", 0, 0),
        CFG_INT("product_id", 0, 0),
        CFG_BOOL("self_powered", cfg_true, 0),
        CFG_BOOL("remote_wakeup", cfg_true, 0),
        CFG_BOOL("in_is_isochronous", cfg_false, 0),
        CFG_BOOL("out_is_isochronous", cfg_false, 0),
        CFG_BOOL("suspend_pull_downs", cfg_false, 0),
        CFG_BOOL("use_serial", cfg_false, 0),
        CFG_BOOL("change_usb_version", cfg_false, 0),
        CFG_INT("usb_version", 0, 0),
        CFG_INT("default_pid", 0x6001, 0),
        CFG_INT("max_power", 0, 0),
        CFG_STR("manufacturer", "Acme Inc.", 0),
        CFG_STR("product", "USB Serial Converter", 0),
        CFG_STR("serial", "08-15", 0),
        CFG_INT("eeprom_type", 0x00, 0),
        CFG_STR("filename", "", 0),
        CFG_BOOL("flash_raw", cfg_false, 0),
        CFG_BOOL("high_current", cfg_false, 0),
        CFG_STR_LIST("cbus0", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus1", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus2", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus3", "{TXDEN,PWREN,RXLED,TXLED,TXRXLED,SLEEP,CLK48,CLK24,CLK12,CLK6,IO_MODE,BITBANG_WR,BITBANG_RD,SPECIAL}", 0),
        CFG_STR_LIST("cbus4", "{TXDEN,PWRON,RXLED,TXLED,TX_RX_LED,SLEEP,CLK48,CLK24,CLK12,CLK6}", 0),
        CFG_BOOL("invert_txd", cfg_false, 0),
        CFG_BOOL("invert_rxd", cfg_false, 0),
        CFG_BOOL("invert_rts", cfg_false, 0),
        CFG_BOOL("invert_cts", cfg_false, 0),
        CFG_BOOL("invert_dtr", cfg_false, 0),
        CFG_BOOL("invert_dsr", cfg_false, 0),
        CFG_BOOL("invert_dcd", cfg_false, 0),
        CFG_BOOL("invert_ri", cfg_false, 0),
        CFG_STR("channel_a_driver", "VCP", 0),
        CFG_STR("channel_b_driver", "VCP", 0),
        CFG_STR("channel_c_driver", "VCP", 0),
        CFG_STR("channel_d_driver", "VCP", 0),
        CFG_BOOL("channel_a_rs485", cfg_false, 0),
        CFG_BOOL("channel_b_rs485", cfg_false, 0),
        CFG_BOOL("channel_c_rs485", cfg_false, 0),
        CFG_BOOL("channel_d_rs485", cfg_false, 0),
        CFG_END()
    };
    cfg_t *cfg;

    /*
    normal variables
    */
    int _decode = 0, _scan = 0, _read = 0, _erase = 0, _flash = 0, _debug = 0;

    const int max_eeprom_size = 256;
    int my_eeprom_size = 0;
    unsigned char *eeprom_buf = NULL;
    char *filename=NULL, *cfg_filename=NULL;
    int size_check;
    int option_vid=0x403, option_pid=0x6001;
    int i, f, return_code=0;
    FILE *fp;

    struct ftdi_context *ftdi = NULL;

		printf ("\nftdi-flash-tool v%s\n", EEPROM_VERSION_STRING);
    printf ("\nAn FTDI eeprom generator\n");
    printf ("(c) Brandon Warhurst\n");

	/* Check the options */
    while ((i = getopt(argc, argv, "dDef:ho:rv:p:s")) != -1) {
		switch(i) {
		case 'd':       /* decode */
			_decode = 1;
			break;
		case 'D':       /* debug */
			_debug = 1;
			break;
		case 'v':       /* VID */
			option_vid = strtoul(optarg, NULL, 0);
			break;
		case 'p':       /* PID */
			option_pid = strtoul(optarg, NULL, 0);
			break;
		case 'o':       /* output (for read) */
			filename = optarg;
			break;
		case 'r':       /* read command */
			_flash = 0; _read = 1; _erase = 0;
			break;
		case 'e':       /* erase command */
			_flash = 0; _read = 0; _erase = 1;
			cfg_filename = NULL;
			filename = NULL;
			break;
		case 'f':       /* flash command */
			_flash = 1; _read = 0; _erase = 0;
			filename = NULL;
			cfg_filename = optarg;
			break;
		case 's':       /* scan command (currently not really useful) */
			_scan = 1;
			break;
		case 'h':       /* help */
		default:
			usage(argv[0]);
		}
    }

        /* Allocate the ftdi structure */
    if ((ftdi = ftdi_new()) == 0)
    {
        fprintf(stderr, "Failed to allocate ftdi structure :%s \n",
                ftdi_get_error_string(ftdi));
        return EXIT_FAILURE;
    }

	if(_scan > 0) {
		/* If we are scanning, do this stuff here. */
		/* Currently doesn't really do anything useful. */
		struct ftdi_device_list *devlist;
		int res;
		if ((res = ftdi_usb_find_all(ftdi, &devlist, 0, 0)) < 0)
		{
			fprintf(stderr, "No FTDI with default VID/PID found\n");
			return_code =  -1;
		}
	
		printf("Found %d default FTDI devices\n",res);
	
		goto cleanup;
	}

	/* Check to make sure a command was provided */
	if(_read == 0 && _flash == 0 && _erase == 0) usage(argv[0]);

    if(_flash > 0) {
		/* if we are flashing... */
	
		printf("Writing...\n");
		if ((fp = fopen(cfg_filename, "r")) == NULL)
		{
			printf ("Can't open configuration file\n");
			return -1;
		}
		fclose (fp);

		cfg = cfg_init(opts, 0);
		cfg_parse(cfg, cfg_filename);
		filename = cfg_getstr(cfg, "filename");

		if (cfg_getbool(cfg, "self_powered") && cfg_getint(cfg, "max_power") > 0)
			printf("Hint: Self powered devices should have a max_power setting of 0.\n");

		int vendor_id = cfg_getint(cfg, "vendor_id");
		int product_id = cfg_getint(cfg, "product_id");
		i = locate_ftdi_device(ftdi,vendor_id,product_id,option_vid,option_pid);
		int target_vid = cfg_getint(cfg, "target_vendor_id");
		int target_pid = cfg_getint(cfg, "target_product_id");
		if(i && (target_vid != option_vid || target_pid != option_pid))
			locate_ftdi_device(ftdi,target_vid, target_pid, option_vid, option_pid);

		if(i != 0) { QUIT; }

		if((f=ftdi_read_eeprom(ftdi))) {
			fprintf(stderr, "FTDI read eeprom: %d (%s)\n", f, ftdi_get_error_string(ftdi));
		}

		i = detect_eeprom(ftdi,cfg_getint(cfg,"eeprom_type"));

		ftdi_eeprom_initdefaults (ftdi, cfg_getstr(cfg, "manufacturer"),
										cfg_getstr(cfg, "product"),
										cfg_getstr(cfg, "serial"));


		eeprom_set_value(ftdi, CHIP_TYPE, i);

		eeprom_get_value(ftdi, CHIP_SIZE, &my_eeprom_size);
		if(my_eeprom_size < 0) {
			if ((i == 0x56) || (i == 0x66))
				my_eeprom_size = 0x100;
			else
				my_eeprom_size = 0x80;
        }

		printf("EEPROM size: %d\n",my_eeprom_size);
		eeprom_set_value(ftdi, VENDOR_ID, cfg_getint(cfg, "vendor_id"));
		eeprom_set_value(ftdi, PRODUCT_ID, cfg_getint(cfg, "product_id"));

		eeprom_set_value(ftdi, SELF_POWERED, cfg_getbool(cfg, "self_powered"));
		eeprom_set_value(ftdi, REMOTE_WAKEUP, cfg_getbool(cfg, "remote_wakeup"));
		eeprom_set_value(ftdi, MAX_POWER, cfg_getint(cfg, "max_power"));

		eeprom_set_value(ftdi, IN_IS_ISOCHRONOUS, cfg_getbool(cfg, "in_is_isochronous"));
		eeprom_set_value(ftdi, OUT_IS_ISOCHRONOUS, cfg_getbool(cfg, "out_is_isochronous"));
		eeprom_set_value(ftdi, SUSPEND_PULL_DOWNS, cfg_getbool(cfg, "suspend_pull_downs"));

		eeprom_set_value(ftdi, USE_SERIAL, cfg_getbool(cfg, "use_serial"));
		eeprom_set_value(ftdi, USE_USB_VERSION, cfg_getbool(cfg, "change_usb_version"));
		eeprom_set_value(ftdi, USB_VERSION, cfg_getint(cfg, "usb_version"));

		eeprom_set_value(ftdi, HIGH_CURRENT, cfg_getbool(cfg, "high_current"));
		eeprom_set_value(ftdi, CBUS_FUNCTION_0, str_to_cbus(cfg_getstr(cfg, "cbus0"), 13));
		eeprom_set_value(ftdi, CBUS_FUNCTION_1, str_to_cbus(cfg_getstr(cfg, "cbus1"), 13));
		eeprom_set_value(ftdi, CBUS_FUNCTION_2, str_to_cbus(cfg_getstr(cfg, "cbus2"), 13));
		eeprom_set_value(ftdi, CBUS_FUNCTION_3, str_to_cbus(cfg_getstr(cfg, "cbus3"), 13));
		eeprom_set_value(ftdi, CBUS_FUNCTION_4, str_to_cbus(cfg_getstr(cfg, "cbus4"), 9));
		int invert = 0;
		if (cfg_getbool(cfg, "invert_rxd")) invert |= INVERT_RXD;
		if (cfg_getbool(cfg, "invert_txd")) invert |= INVERT_TXD;
		if (cfg_getbool(cfg, "invert_rts")) invert |= INVERT_RTS;
		if (cfg_getbool(cfg, "invert_cts")) invert |= INVERT_CTS;
		if (cfg_getbool(cfg, "invert_dtr")) invert |= INVERT_DTR;
		if (cfg_getbool(cfg, "invert_dsr")) invert |= INVERT_DSR;
		if (cfg_getbool(cfg, "invert_dcd")) invert |= INVERT_DCD;
		if (cfg_getbool(cfg, "invert_ri")) invert |= INVERT_RI;
		eeprom_set_value(ftdi, INVERT, invert);

		eeprom_set_value(ftdi, CHANNEL_A_DRIVER, str_to_drvr(cfg_getstr(cfg,"channel_a_driver")) ? DRIVER_VCP : 0);
		eeprom_set_value(ftdi, CHANNEL_B_DRIVER, str_to_drvr(cfg_getstr(cfg,"channel_b_driver")) ? DRIVER_VCP : 0);
		eeprom_set_value(ftdi, CHANNEL_C_DRIVER, str_to_drvr(cfg_getstr(cfg,"channel_c_driver")) ? DRIVER_VCP : 0);
		eeprom_set_value(ftdi, CHANNEL_D_DRIVER, str_to_drvr(cfg_getstr(cfg,"channel_d_driver")) ? DRIVER_VCP : 0);
		eeprom_set_value(ftdi, CHANNEL_A_RS485, cfg_getbool(cfg,"channel_a_rs485"));
		eeprom_set_value(ftdi, CHANNEL_B_RS485, cfg_getbool(cfg,"channel_b_rs485"));
		eeprom_set_value(ftdi, CHANNEL_C_RS485, cfg_getbool(cfg,"channel_c_rs485"));
		eeprom_set_value(ftdi, CHANNEL_D_RS485, cfg_getbool(cfg,"channel_d_rs485"));

		size_check = ftdi_eeprom_build(ftdi);

		if (size_check == -1)
		{
			printf ("Sorry, the eeprom can only contain 128 bytes (100 bytes for your strings).\n");
			printf ("You need to short your string by: %d bytes\n", size_check);
			QUIT;
		} else if (size_check < 0) {
			printf ("ftdi_eeprom_build(): error: %d\n", size_check);
		}
		else
		{
			printf ("Used eeprom space: %d bytes\n", my_eeprom_size-size_check);
		}

		if (cfg_getbool(cfg, "flash_raw"))
		{
			if (filename != NULL && strlen(filename) > 0)
			{
				eeprom_buf = malloc(max_eeprom_size);
				FILE *fp = fopen(filename, "rb");
				if (fp == NULL)
				{
					printf ("Can't open eeprom file %s.\n", filename);
					QUIT;
				}
				my_eeprom_size = fread(eeprom_buf, 1, max_eeprom_size, fp);
				fclose(fp);
				if (my_eeprom_size < 128)
				{
					printf ("Can't read eeprom file %s.\n", filename);
					QUIT;
				}

				ftdi_set_eeprom_buf(ftdi, eeprom_buf, my_eeprom_size);
			}
		}
		if((f=ftdi_write_eeprom(ftdi)))
			printf ("FTDI write eeprom: %d (%s)\n", f,ftdi_get_error_string(ftdi));
		libusb_reset_device(ftdi->usb_dev);

		if(_decode > 0) read_decode_eeprom(ftdi,_debug);

		cfg_free(cfg);

	} else {
		i = locate_ftdi_device(ftdi,option_vid,option_pid, 0x403, 0x6001);
		if(i != 0) { QUIT; }
		if (_read > 0)
		{
			/* if we are reading... */

			printf("Reading...\n");

			if((f=ftdi_read_eeprom(ftdi))) {
				fprintf(stderr, "FTDI read eeprom: %d (%s)\n", f, ftdi_get_error_string(ftdi));
			}

			eeprom_get_value(ftdi, CHIP_SIZE, &my_eeprom_size);

			if(my_eeprom_size > 0) printf("EEPROM size: %d\n", my_eeprom_size);
			else { printf("No EEPROM or EEPROM not programmed.\n"); QUIT; }

			if(_decode > 0) read_decode_eeprom(ftdi,_debug);

			eeprom_buf = malloc(my_eeprom_size);
			ftdi_get_eeprom_buf(ftdi, eeprom_buf, my_eeprom_size);
			if (eeprom_buf == NULL)
			{
				fprintf(stderr, "Malloc failed, aborting\n");
				goto cleanup;
			}
			if (filename != NULL && strlen(filename) > 0)
			{

				FILE *fp = fopen (filename, "wb");
				printf("Writing eeprom data to %s\n",filename);
				fwrite (eeprom_buf, 1, my_eeprom_size, fp);
				fclose (fp);
			}
		} else {
			/* if we are erasing... */
			printf("Erasing...");
			if((f = ftdi_erase_eeprom(ftdi))) {
				printf("\n");
				fprintf(stderr,"FTDI erase eeprom: %d (%s)\n",f, ftdi_get_error_string(ftdi));
				QUIT;
			}
		}
    }

/* Finish up here */
cleanup:
	printf("command complete.\n");
	if (eeprom_buf)
		free(eeprom_buf);
		if((f=ftdi_usb_close(ftdi)))
			printf("FTDI close: %d (%s)\n", f, ftdi_get_error_string(ftdi));

	ftdi_deinit (ftdi);
	ftdi_free (ftdi);

	printf("\n");
	return return_code;
}
