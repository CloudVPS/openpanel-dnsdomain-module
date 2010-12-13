// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/


#include <openpanel-core/moduleapp.h>
#include "domainModule.h"

#include <grace/file.h>
#include <grace/filesystem.h>
#include <grace/http.h>
#include <grace/pcre.h>


APPOBJECT(domainModule);

//  =========================================================================
/// Main method.
//  =========================================================================
int domainModule::main (void)
{
	string conferr;

	// Add configuration watcher
	conf.addwatcher ("config", &domainModule::confSystem);

   // Load will fail if watchers did not valiate.
    if (! conf.load ("openpanel.module.domain", conferr))
    {   
        ferr.printf ("%% Error loading configuration: %s\n", conferr.str());
        return 1;
    }

	// Explicitly do this before checkconfig
 	if(data["OpenCORE:Command"] == "getconfig")
	{
		sendGetConfig();
		return 0;
	}
	
	// If check config failes return 0 from main
	// Error has been written by the function
	// in control
	// If the command was verify this function will return also
	// false but with an OK reply to opencore
	if (! checkconfig (data))
		return 0;
		
	if (data["OpenCORE:Context"] == "System:AXFR")
	{
		handleaxfr (data["OpenCORE:Command"],
					data["OpenCORE:Session"]["objectid"]);
					
		return 0;
	}
		
	statstring domid = data["Domain"]["id"];
	string prizone;
	bool isSlave = false;

	if (data.exists ("DNSDomain:Master"))
	{
		prizone = data["DNSDomain:Master"]["id"];
	}
	else if (data.exists ("DNSDomain:Slave"))
	{
		prizone = data["DNSDomain:Slave"]["id"];
		isSlave = true;
	}

	string domprefix = prizone.copyuntil (domid.sval());

	value zonelist;
	foreach(aliasdoms, data["Domain"]["Domain:Alias"])
	{
		if (domprefix)
		{
			string alias;
			if (domprefix[-1] == '.')
			{
				alias = "%s%s" %format (domprefix, aliasdoms["id"]);
			}
			else
			{
				alias = "%s.%s" %format (domprefix, aliasdoms["id"]);
			}
			zonelist.newval() = alias;
		}
		else
		{
			zonelist.newval() = aliasdoms["id"];
		}
	}
	zonelist.newval() = prizone;
	
	// Convenience vars.
	string conf_varpath = conf["config"]["varpath"];
	string conf_zonepath = conf["config"]["zonepath"];

	value recordclasses = $("DNSDomain:A",true) ->
						  $("DNSDomain:NS",true) ->
						  $("DNSDomain:MX",true) ->
						  $("DNSDomain:CNAME",true) ->
						  $("DNSDomain:AAAA",true) ->
						  $("DNSDomain:TXT",true);
						  
	value domclasses = $("DNSDomain:Master",true) ->
					   $("DNSDomain:Slave", true);
	
	value cudcommands = $("create",true) -> 
						$("delete",true) -> 
						$("update",true);
						
	value cucommands = $("create",true) -> $("update",true);

	statstring cmd = data["OpenCORE:Command"];
	statstring classid = data["OpenCORE:Session"]["classid"];

    // Go over the zones.
	foreach(zonename, zonelist)
	{
		// Path storage.
		string zonefile;
		string zfname, zfrealname;
							 
		// Construct zone config file name
		if (isSlave)
		{
			zonefile = "%s/%s.slavezone" %format (conf_varpath, zonename);
			zfname = "%s.slavezone" %format (zonename);
			zfrealname = "%s/%s.slavezone" %format (conf_zonepath, zonename);
		}
		else
		{
			zonefile = "%s/%s.zone" %format (conf_varpath, zonename);
			zfname = "%s.zone" %format (zonename);
			zfrealname = "%s/%s.zone" %format (conf_zonepath, zonename);
		}
	
		// just write out a new zonefile if
		// (a) we're creating or updating
		// (b) we're deleting single records (which is effectively an update
		//     to a whole DNSDomain anyway)
		
		if ((recordclasses.exists (classid) && cudcommands.exists (cmd)) ||
		    (cucommands.exists (cmd)))
		{
				if (isSlave)
				{
					string masterip = data["DNSDomain:Slave"]["masterip"];
					fs.save (zonefile, masterip);
				}
				else if (! write_zonefile (zonefile, data, classid,
										   zonename.str()))
				{
					return false;
				}

				// Install the file to the bind directory
				if (authd.installfile (zfname, conf_zonepath))
				{
					// Do rollback
					authd.rollback ();
	
					sendresult( moderr::err_authdaemon, 
								"Error installing zone file");
				
					return false;
				}
		}
		else if (cmd == "delete" && (domclasses.exists (classid)))
		{
            unlink(zonefile.str());
			
            authd.deletefile (zfrealname);
		}
		else
		{
			sendresult (moderr::err_command, "Error in command given");
		}	
	}

	// rewrite zones.conf if necessary
	if (domclasses.exists (classid))
	{
		// write fresh zones.conf
		writenamedConf ();
		
		// Install the file to the bind directory
		if( authd.installfile (conf["config"]["zoneconffile"], 
			conf["config"]["zonepath"])
			&& cmd != "delete")
		{
			authd.rollback ();
			sendresult( moderr::err_authdaemon, 
						"Error installing zone config");
		
			return false;
		}
	}
	
	if (authd.reloadservice (conf["config"]["servicename"]) 
	    && cmd != "delete")
	{
		// Do rollback
		authd.rollback ();
		sendresult( moderr::err_authdaemon, 
					"Error reloading service");
	
		return false;
	}
	
	// send quit
	if (authd.quit ()
	    && cmd != "delete")
	{
		sendresult (moderr::err_authdaemon, 
					"Error authd/quit cmd, possible rollback done");
		return false;		
	}

	sendresult (moderr::ok, "");
	return 0;
}

//  =========================================================================
/// METHOD: domainModule::sendGetConfig
//  =========================================================================
void domainModule::sendGetConfig (void)
{
	value dnsdom;
	httpsocket hs;
	
	string defaddr = "192.168.1.1";
	string ipqhtml = hs.get ("http://ipinfodb.com/index.php");
	ipqhtml.cropafter ("<li>IP address : <strong>");
	ipqhtml.cropat ("</strong>");
	if (ipqhtml.strlen() && (ipqhtml.strlen() < 18)) defaddr = ipqhtml;
	value theaddr = $("address", defaddr);
	
	dnsdom = $("DNSDomain:Master",
					$attr("type", "class") ->
					$("$prototype$",
						$attr("type", "object") ->
						$("primaryns", "ns.$prototype$") ->
						$("ttl", 3600) ->
						$("DNSDomain:A",
							$attr("type", "class") ->
							$(
								$attr("type", "object") ->
								$("name", "www") ->
								$merge(theaddr)
							 )->
							$(
								$attr("type", "object") ->
								$("name", "ns") ->
								$merge(theaddr)
							 )->
							$(
								$attr("type","object") ->
								$("name", "mail") ->
								$merge(theaddr)
							 )->
							$(
								$attr("type","object") ->
								$("name", "") ->
								$merge(theaddr)
							 )
						 ) ->
						$("DNSDomain:NS",
							$attr("type","class") ->
							$(
								$attr("type","object") ->
								$("name", "") ->
								$("address", "ns.$prototype$.")
							 )
						 ) ->
						$("DNSDomain:MX",
							$attr("type","class") ->
							$(
								$attr("type","object") ->
								$("name", "") ->
								$("mxpref", 10) ->
								$("address", "mail.$prototype$.")
							 )
						 )
					 )
				);
	
	value out;
	out = $("Domain",
				$attr("type","class") ->
				$("$prototype$",
					$attr("type","object") ->
					$merge(dnsdom)
				 )
		   );
	
	sendresult (moderr::ok, "OK", out);
}

//  =========================================================================
/// domainModule::write_zonefile
//  =========================================================================
bool domainModule::write_zonefile 	(const string &filename, 
									 const value &ibody,
									 const string &context,
									 const string &domainname)
{
	int nextserial = 1;
	
	if ( fs.exists(filename) )
	{
		// Determine the serial number for the existing zone file
		string existingzone = fs.load (filename);
		
		static pcregexp serialfinder("/^(\\d+)\t; Serial$/");
		
		value regexresult;
		if( serialfinder.match( existingzone, regexresult ) )
		{
			nextserial = regexresult[0].ival() + 1;
		}
	}
	
	// From this point all data should already be validated
	// we will write the zone file in the folowing order:
	//
	// TTL
	// SOA RECORD
	// A RECORDS
	// CNAME RECORDS
	// MX RECORDS (INCREMENTAL ORDERED)
	//
	
	file f;
	const value &domain = ibody["DNSDomain:Master"];
	
	// Try to open file for reading	
	if (f.openwrite (filename, 0644))
	{
	
		// File is open for writing

        f.writeln (";#############################################################################");
        f.writeln (";# WARNING: AUTOMATICALLY GENERATED                                          #");
        f.writeln (";# This file was automatically generated by OpenPanel. Manual changes to     #");
        f.writeln (";# this file will be lost the next time this file is generated.              #");
        f.writeln (";#############################################################################");
        f.writeln ("");

		// Write the ttl line
		f.printf ("$TTL\t%u\n\n", domain["ttl"].uval());
	
		// Create zone SOA record
		f.printf ("@ IN SOA %s. hostmaster.%s. (", domain["primaryns"].cval(),
											       domainname.cval());
		
		f.printf ("\n%u\t; Serial\n", 		 nextserial);
		f.printf ("%u\t\t; Refresh\n", 		 16384);
		f.printf ("%u\t\t; Retry\n", 		 2048);
		f.printf ("%u\t\t; Expire\n", 		 1048576);
		f.printf ("%u )\t\t; Minimum\n\n\n", 2560);
	

		// Write all A records
		foreach (v, domain["DNSDomain:NS"])
		{	
			string sname = v["name"];
			if (! sname) sname = "@";
			
			if (sname.strlen() < 30);
				sname.pad (30, ' ');

			f.writeln ("%s IN NS           %s" %format (sname, v["address"]));
		}
		
		f.writeln ("");
		
		foreach (v, domain["DNSDomain:MX"])
		{
			string sname = v["name"];
			if (! sname) sname = "@";
			string mxp = v["mxpref"];

			mxp.pad (9, ' ');
			if (sname.strlen() < 30);
				sname.pad (30, ' ');
				
			f.writeln ("%s IN MX %s %s" %format (sname, mxp, v["address"]));
		}
		
		f.writeln ("");

		// Write all A records
		foreach (v, domain["DNSDomain:A"])
		{	
			string sname = v["name"];
			if (! sname) sname = "@";
			
			if (sname.strlen() < 30);
				sname.pad (30, ' ');

			f.writeln ("%s IN A            %s" %format (sname, v["address"]));
		}

		f.writeln ("");

		foreach (v, domain["DNSDomain:AAAA"])
		{	
			string sname = v["name"];
			if (! sname) sname = "@";
			
			if (sname.strlen() < 30);
				sname.pad (30, ' ');

			f.writeln ("%s IN AAAA         %s" %format (sname, v["address"]));
		}
		
		f.writeln ("");

		foreach (v, domain["DNSDomain:CNAME"])
		{	
			string sname = v["name"];
			if (! sname) sname = "@";
			
			if (sname.strlen() < 30);
				sname.pad (30, ' ');

			f.writeln ("%s IN CNAME        %s" %format (sname, v["address"]));
		}
		
		f.writeln ("");

		foreach (v, domain["DNSDomain:TXT"])
		{	
			string sname = v["name"];
			if (! sname) sname = "@";
			
			if (sname.strlen() < 30);
				sname.pad (30, ' ');

			f.writeln ("%s IN TXT          %s" %format (sname, v["address"]));
		}
		
		// Close zone file
		f.close ();
		
		
		return true;
	}

	// Error writing zone file
	sendresult (moderr::err_writefile, "Error writing zone file");
	return false;	
}



//  =========================================================================
/// domainModule::writenamedconf
//  =========================================================================
bool domainModule::writenamedConf (void)
{
	// Writes a new named config file 
	// into the given filename.
	// This will generated form the value zoneconf

	file f;
	string conffile;

	// Construct config file name
	conffile.printf ("%s/%s", conf["config"]["varpath"].str(),
					 conf["config"]["zoneconffile"].str());

	// Try to open file for reading	
	if (f.openwrite (conffile, 0644))
	{
        f.writeln ("###############################################################################");
        f.writeln ("## WARNING: AUTOMATICALLY GENERATED                                          ##");
        f.writeln ("## This file was automatically generated by OpenPanel. Manual changes to     ##");
        f.writeln ("## this file will be lost the next time this file is generated.              ##");
        f.writeln ("###############################################################################");
        f.writeln ("");
	
	
		f.writeln ("include \"/var/named/openpanel/axfr.conf\";");
		value zonefiles = fs.ls(conf["config"]["varpath"], false);
		foreach (z, zonefiles)
		{
			string p = z.id();
			string q = p.cutatlast(".");
			if (p == "zone")
			{
				f.writeln ("zone \"%s\" {" %format (q));
				f.writeln ("\t type master;");
				f.writeln ("\t file \"%s/%s\";" %format
										(conf["config"]["zonepath"], z.id()));
				f.writeln ("\t allow-transfer { openpanel-axfr; };");
				f.writeln ("};\n");
			}
			else if (p == "slavezone")
			{
				string masterip = fs.load (z["path"]);
				f.writeln ("zone \"%s\" {" %format (q));
				f.writeln ("\t type slave;");
				f.writeln ("\t file \"%s/slaves/%s.slavedata\";"
					%format (conf["config"]["zonepath"], q));
				f.writeln ("\t masters { %s; };" %format (masterip));
				f.writeln ("};\n");
			}
		}
		f.close ();
		
		return true;
	}
	
	// Error writing named config file
	sendresult (moderr::err_writefile, "Error writing zone config");
	return false;
}



//  =========================================================================
/// domainModule::checkconfig
//  =========================================================================
bool domainModule::checkconfig (value &ibody)
{
	value domainclasses = $("DNSDomain:Master",true) ->
						  $("DNSDomain:Slave",true) ->
						  $("System:AXFR",true);
						  
	value recordclasses = $("DNSDomain:A",true) ->
						  $("DNSDomain:MX",true) ->
						  $("DNSDomain:NS",true) ->
						  $("DNSDomain:CNAME",true) ->
						  $("DNSDomain:TXT",true) ->
						  $("DNSDomain:AAAA",true);

	value validclasses = domainclasses;
	validclasses << recordclasses;

	statstring classid = ibody["OpenCORE:Session"]["classid"];

	if (! validclasses.exists (classid))
	{
		sendresult (moderr::err_context,
					"Unknown class '%s'" %format (classid));
		return false;
	}
	
	if (classid == "System:AXFR") return true;

	if (! (ibody.exists ("DNSDomain:Master") ||
		   ibody.exists ("DNSDomain:Slave")))
	{
		sendresult (moderr::err_notfound,
					"No DNSDomain class found in info body");
		return false;
	}
	
	if (domainclasses.exists (classid)) return true;
	
	// Check if the correct object info classes are supplied
	if (ibody["OpenCORE:Session"]["classid"] == "DNSDomain")
	{		
		return true;
	}
	
	// All necessary classes are found,
	// next step is to check all given values which are 
	// at least required to process this request as valid
	
	// Loop through all domain records
	// and validate the given data
	// TODO: perhaps change this to just verify the newly added/updated record
	foreach (rectype, recordclasses)
	{
		foreach (rec, ibody["DNSDomain:Master"][rectype.id()])
		{
			if (! rec.exists ("address"))
			{
				sendresult (moderr::err_notfound, "No address field");
				return false;
			}
			if (rectype == "DNSDomain::MX")
			{
				if (! rec.exists ("mxpref"))
				{
					sendresult (moderr::err_notfound, "No mxpref");
					return false;
				}
			}
		}
	}
		
	return true;
}



//  =========================================================================
/// METHOD: domainModule::recordexists 
//  =========================================================================
bool domainModule::recordexists (const value &v, const string &fname)
{
	if (! v.exists (fname))
	{
		string errval = "Missing required field: %s" %format (fname);
		sendresult (moderr::err_notfound, errval);
		
		return false;
	}
	
	return true;
}


//  =========================================================================
/// Configuration watcher for the event log.
//  =========================================================================
bool domainModule::confSystem (config::action act, keypath &kp,
                const value &nval, const value &oval)
{
	switch (act)
	{
		case config::isvalid:
			return true;

		case config::create:
			return true;		
	}

	return false;
}

void domainModule::handleaxfr (const string &cmd, const statstring &id)
{
	value data;
	file f;
	
	if (f.openread ("/var/named/openpanel/axfr.conf"))
	{
		while (! f.eof ())
		{
			string ln = f.gets ();
			if (ln.strstr ("acl openpanel-axfr") >= 0) continue;
			if (ln.strstr ("};") >= 0) continue;
			if (ln && ln[0] == '\t')
			{
				ln.cropafterlast ('\t');
				ln.cropat (';');
				data[ln] = true;
			}
		}
		f.close ();
	}
	
	caseselector (cmd)
	{
		incaseof ("create") :
			data[id] = true;
			break;
		
		incaseof ("delete") :
			data.rmval (id);
			break;
		
		defaultcase :
			sendresult (moderr::err_context, "Invalid action");
			return;
	}
	
	if (! data.count()) data["127.0.0.1"] = true;
	
	f.openwrite ("/var/openpanel/conf/staging/DNSDomain/axfr.conf");

    f.writeln ("###############################################################################");
    f.writeln ("## WARNING: AUTOMATICALLY GENERATED                                          ##");
    f.writeln ("## This file was automatically generated by OpenPanel. Manual changes to     ##");
    f.writeln ("## this file will be lost the next time this file is generated.              ##");
    f.writeln ("###############################################################################");
    f.writeln ("");
	
	f.writeln ("acl openpanel-axfr {");
	foreach (d, data)
	{
		f.writeln ("\t%s;" %format (d.id()));
	}
	f.writeln ("};");
	f.close ();
	
	if (authd.installfile ("axfr.conf", "/var/named/openpanel"))
	{
		sendresult (moderr::err_writefile, "Error installing axfr.conf file");
		return;
	}
	
	sendresult (moderr::ok, "OK");
}
