
# Note: confxml.cpp is in src/utils. You can build it with:
#    c++ -std=c++17 -I. confxml.cpp -o confxml -lrecoll


recoll.conf.5: recoll.conf.5-header ../../sampleconf/recoll.conf
	(cat recoll.conf.5-header && confxml --man ../../sampleconf/recoll.conf; cat recoll.conf.5-trailer) > recoll.conf.5
