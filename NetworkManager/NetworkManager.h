
#ifndef NETWORK_MANAGER_LIBRARY_H
#define NETWORK_MANAGER_LIBRARY_H

#pragma once

#ifdef NETWORK_MANAGER_LIBRARY_EXPORTS
#define NETWORK_MANAGER_LIBRARY_ __declspec(dllexport)
#else
#define NETWORK_MANAGER_LIBRARY_ __declspec(dllimport)
#endif


class NetworkManager {

};


#endif