#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../utilsdb/rts2appdb.h"
#include "../utils/rts2config.h"
#include "rts2selector.h"

#include <iostream>

class Rts2SelectorApp:public Rts2AppDb
{
public:
  Rts2SelectorApp (int argc, char **argv);
    virtual ~ Rts2SelectorApp (void);
  virtual int run ();
};

Rts2SelectorApp::Rts2SelectorApp (int in_argc, char **in_argv):
Rts2AppDb (in_argc, in_argv)
{
}

Rts2SelectorApp::~Rts2SelectorApp (void)
{
}

int
Rts2SelectorApp::run ()
{
  int next_tar;

  Rts2Config *config;
  struct ln_lnlat_posn *observer;

  Rts2Selector *sel;

  Target *tar;

  config = Rts2Config::instance ();
  observer = config->getObserver ();

  sel = new Rts2Selector (observer);

  next_tar = sel->selectNextNight ();

  std::cout << "Next target:" << next_tar << std::endl;

  tar = createTarget (next_tar, observer);
  if (tar)
    {
      std::cout << *tar << std::endl;
    }
  else
    {
      std::cout << "cannot create target" << std::endl;
    }

  delete tar;

  delete sel;
  return 0;
}

int
main (int argc, char **argv)
{
  Rts2SelectorApp selApp = Rts2SelectorApp (argc, argv);
  int ret = selApp.init ();
  if (ret)
    return ret;
  return selApp.run ();
}
