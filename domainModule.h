// This file is part of OpenPanel - The Open Source Control Panel
// OpenPanel is free software: you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the Free 
// Software Foundation, using version 3 of the License.
//
// Please note that use of the OpenPanel trademark may be subject to additional 
// restrictions. For more information, please visit the Legal Information 
// section of the OpenPanel website on http://www.openpanel.com/

#ifndef _domainModule_H
#define _domainModule_H 1

#include <openpanel-core/moduleapp.h>
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
	
	void	 handleaxfr (const string &cmd, const statstring &id);
	
};

#endif
