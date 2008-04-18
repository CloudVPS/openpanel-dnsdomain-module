// --------------------------------------------------------------------------
// OpenPanel - The Open Source Control Panel
// Copyright (c) 2006-2007 PanelSix
//
// This software and its source code are subject to version 2 of the
// GNU General Public License. Please be aware that use of the OpenPanel
// and PanelSix trademarks and the IconBase.com iconset may be subject
// to additional restrictions. For more information on these restrictions
// and for a copy of version 2 of the GNU General Public License, please
// visit the Legal and Privacy Information section of the OpenPanel
// website on http://www.openpanel.com/
// --------------------------------------------------------------------------

#ifndef _domainModule_H
#define _domainModule_H 1

#include <moduleapp.h>
#include <grace/system.h>
#include <grace/configdb.h>


typedef configdb<class domainModule> appconfig;

//  -------------------------------------------------------------------------
/// Main application class.
//  -------------------------------------------------------------------------
class domainModule : public moduleapp
{
public:
		 	 domainModule (void) :
				moduleapp ("openpanel.module.domain"),
				conf (this)
			 {
			 }
			~domainModule (void)
			 {
			 }

	int		 main (void);


	
protected:

	appconfig		conf;		///< Modules configuration data

			 //	 =============================================
			 /// push $template$ to getconfig
			 //	 =============================================
	void	 sendGetConfig (void);

			 //	 =============================================
			 /// Configuration handler 
			 //	 =============================================
	bool     confSystem (config::action act, keypath &path, 
					  	 const value &nval, const value &oval);


			 //	 =============================================
			 /// Write the given config to a given file name
			 /// \param filename file used to write config
			 /// \return true on ok / false on failure
			 //  =============================================
	bool	 write_zonefile 	(const string &,
								 const value &,
								 const string&,
								 const string&);

	bool 	 writenamedConf (void);

			 //	=============================================
			 /// validate the given configuration
			 /// \return true on ok / false on failure
			 //	=============================================
	bool	 checkconfig (value &ibody);

			 // checkconfig helper function
	bool 	 recordexists (const value &v, const string &fname);
	
};

#endif
