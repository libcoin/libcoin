/*
 *  btc.cp
 *  btc
 *
 *  Created by Michael Grønager on 20/09/11.
 *  Copyright 2011 NDGF / NORDUnet A/S. All rights reserved.
 *
 */

#include <iostream>
#include "btc.h"
#include "btcPriv.h"

void btc::HelloWorld(const char * s)
{
	 btcPriv *theObj = new btcPriv;
	 theObj->HelloWorldPriv(s);
	 delete theObj;
};

void btcPriv::HelloWorldPriv(const char * s) 
{
	std::cout << s << std::endl;
};

