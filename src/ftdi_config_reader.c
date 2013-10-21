/***************************************************************************
                       ftdi_config_reader.c  -  description
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml2/libxml/xmlreader.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/xpath.h>
#include <time.h>

int self_powered = 0;
int use_serial_number = 0;

/**
 * @brief Detect if provided node is a leaf node
 *
 * \param node xmlNodePtr to node to be checked
 *
 * Function returns a 1 if node is a leaf node, zero otherwise.
 * 
 **/
int
isLeaf(xmlNodePtr node) {
	xmlNodePtr cur_node = NULL;
	
	for(cur_node = node->children; cur_node; cur_node = cur_node->next) {
		if(cur_node->type == XML_ELEMENT_NODE) return 0;
	}

	return 1;
}

/**
 * @brief Check type of node we are given and output the appropriate string.
 *
 * \param node xmlNodePtr to process, filename - output configuration file.
 *
 * Function Determines what type of node it is and outputs the appropriate text string
 *          to the output configuration file.
 * 
 **/
void
process_node(xmlNodePtr node, const char *filename) {

	FILE *f;
	
	f = fopen(filename, "a+");
	if(f == NULL) {
		perror("Can't produce configuration file.");
		exit(-1);
	}
	
	if(!strcmp(node->name,"Type"))
		fprintf(f, "type=\"%s\"\n", xmlNodeGetContent(node));
	else if(!strcmp(node->name,"idVendor"))
		fprintf(f, "vendor_id=0x%s\n", xmlNodeGetContent(node));
	else if(!strcmp(node->name,"idProduct"))
		fprintf(f, "product_id=0x%s\n", xmlNodeGetContent(node));
	else if(!strcmp(node->name,"SerialNumber_Enabled"))
		if(!strcmp(xmlNodeGetContent(node),"true")) {
			use_serial_number = 1;
			fprintf(f, "use_serial=true\t\t\t# Use the serial number string\n");
		} else {
			fprintf(f, "use_serial=false\t\t# Use the serial number string\n");
		}
	else if(!strcmp(node->name,"SerialNumber"))
		fprintf(f, "serial=\"%s\"\t\t# Serial\n",xmlNodeGetContent(node));
	else if(!strcmp(node->name,"SelfPowered"))
		if(!strcmp(xmlNodeGetContent(node),"true")) {
			fprintf(f, "max_power=0\t\t# Max. power consumption: value * 2 mA. Use 0 if self_powered = true.\n");
			fprintf(f, "self_powered=true\t\t# Turn this off for bus powered\n");
			self_powered = 1;
		} else {
			fprintf(f, "self_powered=false\t\t# Turn this off for bus powered\n");
		}
	else if(!strcmp(node->name,"MaxPower")) {
		if(!self_powered)
			fprintf(f, "max_power=%s\t\t\t# Max. power consumption: value * 2 mA. Use 0 if self_powered = true.\n", xmlNodeGetContent(node));
	} else if(!strcmp(node->name,"IOpullDown"))
		fprintf(f, "suspend_pull_downs=%s\t# Enable suspend pull downs for lower power\n",xmlNodeGetContent(node));
	else if(!strcmp(node->name,"bcdUSB")) {
		;
	} else if(!strcmp(node->name, "Product_Description"))
		fprintf(f, "product=\"%s\"\t\t# Product\n",xmlNodeGetContent(node));
	else if(strstr(xmlGetNodePath(node),"Port_A")) {
		if(!strcmp(node->name,"Virtual_Com_Port") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_a_driver=VCP\t\t# Use VCP driver\n");
		if(!strcmp(node->name,"D2XX_Direct") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_a_driver=D2XX\t\t# Use D2XX driver\n");
	} else if(strstr(xmlGetNodePath(node),"Port_B")) {
		if(!strcmp(node->name,"Virtual_Com_Port") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_b_driver=VCP\t\t# Use VCP driver\n");
		if(!strcmp(node->name,"D2XX_Direct") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_b_driver=D2XX\t\t# Use D2XX driver\n");
	} else if(strstr(xmlGetNodePath(node),"Port_C")) {
		if(!strcmp(node->name,"Virtual_Com_Port") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_c_driver=VCP\t\t# Use VCP driver\n");
		if(!strcmp(node->name,"D2XX_Direct") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_c_driver=D2XX\t\t# Use D2XX driver\n");
	} else if(strstr(xmlGetNodePath(node),"Port_D")) {
		if(!strcmp(node->name,"Virtual_Com_Port") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_d_driver=VCP\t\t# Use VCP driver\n");
		if(!strcmp(node->name,"D2XX_Direct") && !strcmp(xmlNodeGetContent(node),"true"))
			fprintf(f, "channel_d_driver=D2XX\t\t# Use D2XX driver\n");
	}
	

	fclose(f);
}

/**
 * @brief For leaf nodes it executes process_node.  For non leaf nodes, it
 *         continues walking down the XML tree.
 *
 * \param a_node xmlNodePtr to process, filename - name of input file.
 *
 * Function walks an XML tree if a_node is not a leaf node, otherwise, runs
 *          process_node on a_node.
 * 
 **/
void
process_elements(xmlNodePtr a_node, const char *filename) {
	xmlNode *cur_node = NULL;
	char cur_prefix[20];
	
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (isLeaf(cur_node)) {
			if(!xmlIsBlankNode(cur_node)) {
				process_node(cur_node, filename);
				printf("Node type: Element, name: %s,%s<%s> --\n",cur_node->name,xmlGetNodePath(cur_node),xmlNodeGetContent(cur_node), xmlGetNodePath(cur_node));
			}
		} else {
			process_elements(cur_node->children,filename);
		}
		
		/*print_element_names(cur_node->children); */
	}
}

/**
 * @brief Generate the output configuration file.
 *
 * \param filename - input file to process, cfg_file - output file to write to.
 *
 * Function walks the XML tree and processes only the leaf nodes.  Probably not
 *          an ideal method, but it does get the job done.
 * 
 **/
void
generate_configuration(const char* filename,const char* cfg_file) {
	xmlDocPtr	doc;
	xmlNodePtr root_element = NULL;
	xmlXPathContextPtr xpathCtx;
	xmlXPathObjectPtr xpathObj;
	time_t t = time(NULL);
	int i;

	FILE *f;
	
	f = fopen(cfg_file, "w");
	printf("Generating configuration file...\n");
	fprintf(f,"# File: %s\n# Time: %s",cfg_file,asctime(localtime(&t)));
	fprintf(f,"##################################\n");
	fprintf(f,"#  ftdi-xform-config generated   #\n");
	fprintf(f,"#  configuration File            #\n");
	fprintf(f,"#  vvvvvvvvvvvvvvvvvvvvvvvvvvvv  #\n");
	fprintf(f,"##################################\n\n");
	if(f == NULL) {
		perror("opening output config file");
		return;
	}
	fclose(f);

	doc = xmlParseFile(filename);
	if(doc == NULL) {
		fprintf(stderr, "Failed to parse %s\n", filename);
		return;
	}

/*	
	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL) {
		fprintf(stderr, "Error: Unable to create new XPath context\n");
		xmlFreeDoc(doc);
		return;
	}

	xpathObj = xmlXPathEvalExpression("/FT_EEPROM/Hardware_Specific/Port_A", xpathCtx);
	if(xpathObj == NULL) {
		fprintf(stderr, "Error: unable to evaluate xpath expresssion \"%s\"\n", "/FT_EEPROM/Hardware_Specific/Port_A");
		xmlXPathFreeContext(xpathCtx);
		xmlFreeDoc(doc);
		return;
	}
*/
	root_element = xmlDocGetRootElement(doc);
	process_elements(root_element,cfg_file);
	
	f = fopen(cfg_file, "a+");
	printf("Appending additional elements...\n");
	fprintf(f,"\n\n##################################\n");
	fprintf(f,"#  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^  #\n");
	fprintf(f,"#  ftdi-xform-config generated   #\n");
	fprintf(f,"#  configuration File            #\n");
	fprintf(f,"##################################\n\n");
	if(f == NULL) {
		perror("opening output config file");
		return;
	}
	fprintf(f,"###########\n");
	fprintf(f,"# Options #\n");
	fprintf(f,"###########\n");
	fprintf(f,"\n# Adjust your eeprom type for your configuration");
	fprintf(f,"\neeprom_type=?\n\n");

	fprintf(f,"########\n");
	fprintf(f,"# Misc #\n");
	fprintf(f,"########\n");
	fprintf(f,"\n# Add a filename here if you need to save your binary");
	fprintf(f,"\nfilename=?\n\n");
	fclose(f);
	
	printf("Please check the new configuration file and modify as needed.\n");
	
//	for(i = xpathObj->nodesetval->nodeNr-1; i >= 0; i--)
//		print_element_names(xpathObj->nodesetval->nodeTab[i],"");
	

	xmlFreeDoc(doc);
}

/**
 * @brief Output the program's usage.
 *
 * \param program a c string containing the program path.
 *
 * Function outputs a usage string.
 * 
 **/
void
usage(char *program) {
	printf("\n%s <input FTDI xml configuration> <output configuration file>\n\n", program);
	printf("NOTE: Output configuration file is also compatible with ftdi_eeprom.\n");
	printf("      When using this tool with ftdi_eeprom, please change your eeprom_type.\n");
	printf("      This will avoid any potential \"bricking\" issues with ftdi_eeprom.\n");
	printf("      ftdi-flash-tool hopefully avoids the issue by detecting the necessary info\n");
	printf("      if it isn't provided.\n");
	exit(1);
}

/**
 * @brief Convert an FTDI style EEPROM configuration file to an ftdi-flash-tool/ftdi_eeprom
 *         style file.
 *
 * \param argc - number of arguments, argv - argument list.
 *
 * 
 **/
int main(int argc, char **argv) {

		printf ("\nftdi-xform-config v%s\n", EEPROM_VERSION_STRING);
    printf ("\nAn FTDI eeprom configuration conversion tool\n");
    printf ("(c) Brandon Warhurst\n");

	if (argc < 2)
		usage(argv[0]);

	xmlInitParser();
	
	LIBXML_TEST_VERSION

	generate_configuration(argv[1],argv[2]);

	xmlCleanupParser();

	xmlMemoryDump();

	return(0);
}
