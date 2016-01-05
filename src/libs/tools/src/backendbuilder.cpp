/**
 * @file
 *
 * @brief Implementation of backend builder
 *
 * @copyright BSD License (see doc/COPYING or http://www.libelektra.org)
 *
 */



#include <backend.hpp>
#include <backends.hpp>
#include <backendbuilder.hpp>
#include <plugindatabase.hpp>


#include <kdbmodule.h>
#include <kdbplugin.h>
#include <kdbprivate.h>
#include <helper/keyhelper.hpp>

#include <set>
#include <algorithm>

#include <kdb.hpp>
#include <cassert>


using namespace std;


namespace kdb
{


namespace tools
{


BackendBuilderInit::BackendBuilderInit() :
	pluginDatabase(make_shared<ModulesPluginDatabase>()),
	backendFactory("backend")
{
}


BackendBuilderInit::BackendBuilderInit(PluginDatabasePtr const & plugins) :
	pluginDatabase(plugins),
	backendFactory("backend")
{
}

BackendBuilderInit::BackendBuilderInit(BackendFactory const & bf) :
	pluginDatabase(make_shared<ModulesPluginDatabase>()),
	backendFactory(bf)
{
}

BackendBuilderInit::BackendBuilderInit(PluginDatabasePtr const & plugins, BackendFactory const & bf) :
	pluginDatabase(plugins),
	backendFactory(bf)
{
}

BackendBuilderInit::BackendBuilderInit(BackendFactory const & bf, PluginDatabasePtr const & plugins) :
	pluginDatabase(plugins),
	backendFactory(bf)
{
}


BackendBuilder::BackendBuilder(BackendBuilderInit const & bbi) :
	pluginDatabase(bbi.getPluginDatabase()),
	backendFactory(bbi.getBackendFactory())
{
}


BackendBuilder::~BackendBuilder()
{
}

MountBackendBuilder::MountBackendBuilder(BackendBuilderInit const & bbi) :
	BackendBuilder (bbi)
{
}

/**
 * @brief Parse a string containing information to create a KeySet
 *
 * @param pluginArguments comma (,) to separate key=value, contains no whitespaces
 *
 * @return newly created keyset with the information found in the string
 */
KeySet BackendBuilder::parsePluginArguments (std::string const & pluginArguments)
{
	KeySet ks;
	istringstream sstream(pluginArguments);

	std::string keyName;
	std::string value;

	// read until the next '=', this will be the keyname
	while (std::getline (sstream, keyName, '='))
	{
		// read until a ',' or the end of line
		// if nothing is read because the '=' is the last character
		// in the config string, consider the value empty
		if (!std::getline (sstream, value, ',')) value = "";

		ks.append (Key("user/"+keyName, KEY_VALUE, value.c_str(), KEY_END));
	}
	return ks;
}

/**
 * @brief Parse a complete commandline
 *
 * @param cmdline contains space separated plugins with optional plugin configurations
 *
 * @return a parsed PluginSpecVector
 */
PluginSpecVector BackendBuilder::parseArguments (std::string const & cmdline)
{
	// split cmdline
	PluginSpecVector arguments;
	std::string argument;
	istringstream sstream(cmdline);
	while (std::getline (sstream, argument, ' '))
	{
		if (argument.empty()) continue;
		if (std::all_of(argument.begin(), argument.end(),
			[](char c) {return std::isspace(c) || c==',';})) continue;
		if (argument.find ('=') == string::npos)
		{
			PluginSpec ps (argument);
			if (argument.find ('#') == string::npos)
			{
				size_t nr = std::count_if(arguments.begin(), arguments.end(),
						[&argument](PluginSpec const & spec) {return spec.getName() == argument;});
				ps.setRefNumber(nr);
				arguments.push_back(ps);
			} else {
				arguments.push_back(ps);
			}
		} else {
			if (arguments.empty()) throw ParseException ("config for plugin ("+argument+") without previous plugin name");
			arguments.back().appendConfig(parsePluginArguments (argument));
		}
	}

	// fix refnames of single occurrences for backwards compatibility
	for (auto & a: arguments)
	{
		size_t nr = std::count_if(arguments.begin(), arguments.end(),
				[&a](PluginSpec const & spec) {return spec.getName() == a.getName();});
		if (nr == 1 && a.getRefName() == "0")
		{
			a.setRefName (a.getName());
		}

		size_t identical = std::count(arguments.begin(), arguments.end(), a);
		if (identical > 1)
		{
			throw ParseException ("identical plugin found: " + a.getFullName());
		}
	}
	return arguments;
}

/**
 * @brief Makes sure that ordering constraints are fulfilled.
 *
 * @pre a sorted list except of the last element to be inserted
 * @post the last element will be moved to a place where it does not produce an order violation
 *
 * @note its still possible that an order violation is present in the case
 *       of order violation in the other direction (no cycle detection).
 */
void BackendBuilder::sort()
{
	auto hasOrderingConflict = [this] (PluginSpec const & other, PluginSpec const & inserted)
	{
		std::stringstream ss (pluginDatabase->lookupInfo(inserted, "ordering"));
		std::string order;
		while (ss >> order)
		{
			if (order == other.getName())
			{
				return true;
			}

			if (order == pluginDatabase->lookupInfo(other, "provides"))
			{
				return true;
			}
		}
		return false;
	};

	// compare with all but the last
	for (auto i = toAdd.begin(); std::next(i) != toAdd.end(); ++i)
	{
		if (hasOrderingConflict(*i, toAdd.back()))
		{
			// bring last *before* the plugin where ordering constraint
			// would be violated
			PluginSpecVector n(toAdd.begin(), i);
			n.push_back(toAdd.back());
			n.insert(n.end(), i, std::prev(toAdd.end()));
			toAdd = n;
			return;
		}
	}
}

/**
 * @brief resolve all needs that were not resolved by adding plugins.
 *
 * @see addPlugin()
 */
void BackendBuilder::resolveNeeds()
{
	// load dependency-plugins immediately
	for (auto const & ps : toAdd)
	{
		auto plugins = parseArguments(pluginDatabase->lookupInfo(ps, "plugins"));
		for (auto const & plugin : plugins)
		{
			addPlugin (plugin);
		}
	}

	std::vector<std::string> needs;
	do {
		needs.clear();

		// collect everything that is needed
		for (auto const & ps : toAdd)
		{
			std::stringstream ss (pluginDatabase->lookupInfo(ps, "needs"));
			std::string need;
			while (ss >> need)
			{
				needs.push_back(need);
			}
		}

		for (auto const & ps : toAdd)
		{
			// remove the needed plugins that are already inserted
			needs.erase(std::remove(needs.begin(), needs.end(), ps.getName()), needs.end());

			// remove what is already provided
			std::string provides = pluginDatabase->lookupInfo(ps, "provides");
			std::istringstream ss (provides);
			std::string toRemove;
			while (ss >> toRemove)
			{
				needs.erase(std::remove(needs.begin(), needs.end(), toRemove), needs.end());
			}
		}

		// leftover in needs is what is still needed
		// will add one of them:
		if (!needs.empty())
		{
			addPlugin (PluginSpec(needs[0]));
			needs.erase(needs.begin());
		}
	} while (!needs.empty());
}

/**
 * @brief Add a plugin.
 *
 * @pre Needs to be a unique new name (use refname if you want to add the same module multiple times)
 *
 * Will automatically resolve virtual plugins to actual plugins.
 *
 * @see resolveNeeds()
 * @param plugin
 */
void BackendBuilder::addPlugin (PluginSpec const & plugin)
{
	for (auto & p : toAdd)
	{
		if (p == plugin)
		{
			throw PluginAlreadyInserted();
		}
	}

	PluginSpec newPlugin = plugin;

	// if the plugin is actually a provider use it (otherwise we will get our name back):
	PluginSpec provides = pluginDatabase->lookupProvides (plugin.getName());
	if (provides.getName() != newPlugin.getName())
	{
		// keep our config and refname
		newPlugin.setName (provides.getName());
		newPlugin.appendConfig (provides.getConfig());
	}

	toAdd.push_back(newPlugin);
	sort();
}

void BackendBuilder::remPlugin (PluginSpec const & plugin)
{
	toAdd.erase(std::remove(toAdd.begin(), toAdd.end(), plugin));
}

void BackendBuilder::fillPlugins(BackendInterface & b) const
{
	for (auto const & plugin: toAdd)
	{
		b.addPlugin (plugin);
	}
}


void MountBackendBuilder::status (std::ostream & os) const
{
	try {
		MountBackendInterfacePtr b = getBackendFactory().create();
		fillPlugins(*b);
		return b->status (os);
	}
	catch (std::exception const & pce)
	{
		os << "Could not successfully add plugin: " << pce.what() << std::endl;
	}
}

bool MountBackendBuilder::validated () const
{
	try {
		MountBackendInterfacePtr b = getBackendFactory().create();
		fillPlugins(*b);
		return b->validated();
	}
	catch (...)
	{
		return false;
	}
}

void MountBackendBuilder::setMountpoint (Key mountpoint_, KeySet mountConf_)
{
	mountpoint = mountpoint_;
	mountConf = mountConf_;

	MountBackendInterfacePtr mbi = getBackendFactory().create();
	mbi->setMountpoint (mountpoint, mountConf);
}

std::string MountBackendBuilder::getMountpoint() const
{
	return mountpoint.getName();
}

void MountBackendBuilder::setBackendConfig (KeySet const & ks)
{
	backendConf = ks;
}

void MountBackendBuilder::useConfigFile (std::string file)
{
	configfile = file;

	MountBackendInterfacePtr b = getBackendFactory().create();
	bool checkPossible = false;
	for (auto const & p : *this)
	{
		if ("resolver" == getPluginDatabase()->lookupInfo(p, "provides"))
		{
			checkPossible = true;
		}
	}

	if (!checkPossible) return;
	fillPlugins (*b);
	b->useConfigFile (configfile);
}

std::string MountBackendBuilder::getConfigFile() const
{
	return configfile;
}

void MountBackendBuilder::serialize (kdb::KeySet &ret)
{
	MountBackendInterfacePtr mbi = getBackendFactory().create();
	fillPlugins (*mbi);
	mbi->setMountpoint (mountpoint, mountConf);
	mbi->setBackendConfig (backendConf);
	mbi->useConfigFile (configfile);
	mbi->serialize(ret);
}

}

}
