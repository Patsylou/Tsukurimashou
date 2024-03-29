%
% Unicode page 90 (Kanji) for Tsukurimashou
% Copyright (C) 2011, 2012, 2013, 2014  Matthew Skala
%
% This program is free software: you can redistribute it and/or modify
% it under the terms of the GNU General Public License as published by
% the Free Software Foundation, version 3.
%
% As a special exception, if you create a document which uses this font, and
% embed this font or unaltered portions of this font into the document, this
% font does not by itself cause the resulting document to be covered by the
% GNU General Public License. This exception does not however invalidate any
% other reasons why the document might be covered by the GNU General Public
% License. If you modify this font, you may extend this exception to your
% version of the font, but you are not obligated to do so. If you do not
% wish to do so, delete this exception statement from your version.
%
% This program is distributed in the hope that it will be useful,
% but WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
% GNU General Public License for more details.
%
% You should have received a copy of the GNU General Public License
% along with this program.  If not, see <http://www.gnu.org/licenses/>.
%
% Matthew Skala
% http://ansuz.sooke.bc.ca/
% mskala@ansuz.sooke.bc.ca
%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

beginfont

% AUTODEPS
input bkencl.mp;
input bottomrad.mp;
input buildkanji.mp;
input gradeeight.mp;
input gradefive.mp;
input gradefour.mp;
input gradenine.mp;
input gradeone.mp;
input gradesix.mp;
input gradethree.mp;
input gradetwo.mp;
input leftrad.mp;
input radical.mp;
input rare.mp;
input rightrad.mp;
input toprad.mp;

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grtwo.distant =
  push_pbox_toexpand("kanji.grtwo.distant");
  build_kanji.road_enclose(tsu_xform(identity shifted (0,-20))
    (kanji.radical.long_kimono));
  expand_pbox;
enddef;

vardef kanji.grtwo.street =
  push_pbox_toexpand("kanji.grtwo.street");
  build_kanji.road_enclose(kanji.grtwo.neck);
  expand_pbox;
enddef;

vardef kanji.grtwo.traffic =
  push_pbox_toexpand("kanji.grtwo.traffic");
  build_kanji.road_enclose(kanji.rare.walled_road);
  expand_pbox;
enddef;

vardef kanji.grtwo.week =
  push_pbox_toexpand("kanji.grtwo.week");
  build_kanji.road_enclose(kanji.grfour.circuit);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grthree.fast =
  push_pbox_toexpand("kanji.grthree.fast");
  build_kanji.road_enclose(kanji.grfour.bundle);
  expand_pbox;
enddef;

vardef kanji.grthree.fortune =
  push_pbox_toexpand("kanji.grthree.fortune");
  build_kanji.road_enclose(kanji.grfour.army);
  expand_pbox;
enddef;

vardef kanji.grthree.metropolis =
  push_pbox_toexpand("kanji.grthree.metropolis");
  build_kanji.lr(580,0)
    (kanji.grthree.person)
    (kanji.rightrad.beta);
  expand_pbox;
enddef;

vardef kanji.grthree.play =
  push_pbox_toexpand("kanji.grthree.play");
  build_kanji.road_enclose(kanji.rare.swim);
  expand_pbox;
enddef;

vardef kanji.grthree.proceed =
  push_pbox_toexpand("kanji.grthree.proceed");
  build_kanji.road_enclose(kanji.radical.old_bird);
  expand_pbox;
enddef;

vardef kanji.grthree.section =
  push_pbox_toexpand("kanji.grthree.section");
  build_kanji.lr(580,0)
    (build_kanji.tb(340,-60+20*mincho)
       (kanji.toprad.stand_up)
       (kanji.grone.mouth))
    (kanji.rightrad.beta);
  expand_pbox;
enddef;

vardef kanji.grthree.send =
  push_pbox_toexpand("kanji.grthree.send");
  build_kanji.road_enclose(build_kanji.level(
    build_kanji.tb(600,220)
      (kanji.toprad.sparkle_two)
      (kanji.grfour.husband);
    replace_strokep(-1)(subpath
      (xpart (oldp intersectiontimes get_strokep(-3)),infinity) of oldp);
    set_boserif(-1,0,whatever);
    replace_strokep(0)((point 1.7 of get_strokep(-1))..(690,100)..(850,-40));
    replace_strokeq(0)((0.9,0.9)--(1.5,1.5)--(1.8,1.8));
  ));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.choose =
  push_pbox_toexpand("kanji.grfour.county");
  build_kanji.road_enclose(kanji.grnine.southeast);
  expand_pbox;
enddef;

vardef kanji.grfour.county =
  push_pbox_toexpand("kanji.grfour.county");
  build_kanji.lr(580,0)
    (kanji.grthree.junior)
    (kanji.rightrad.beta);
  expand_pbox;
enddef;

vardef kanji.grfour.plural =
  push_pbox_toexpand("kanji.grfour.plural");
  build_kanji.road_enclose(build_kanji.tb(560,0)
    (kanji.grone.earth)
    (kanji.grthree.sheep));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfive.make_up =
  push_pbox_toexpand("kanji.grfive.make_up");
  build_kanji.road_enclose(kanji.grfour.revelation);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.greight.avoid =
  push_pbox_toexpand("kanji.greight.avoid");
  build_kanji.road_enclose(kanji.rare.punish);
  expand_pbox;
enddef;

vardef kanji.greight.death =
  push_pbox_toexpand("kanji.greight.death");
  build_kanji.road_enclose(kanji.grfour.fold);
  expand_pbox;
enddef;

vardef kanji.greight.escape =
  push_pbox_toexpand("kanji.greight.escape");
  build_kanji.road_enclose(kanji.grfour.portent);
  expand_pbox;
enddef;

vardef kanji.greight.follow =
  push_pbox_toexpand("kanji.greight.follow");
  build_kanji.road_enclose(kanji.radical.pig);
  expand_pbox;
enddef;

vardef kanji.greight.humble =
  push_pbox_toexpand("kanji.greight.humble");
  build_kanji.road_enclose(kanji.grfour.grandchild);
  expand_pbox;
enddef;

vardef kanji.greight.retard =
  push_pbox_toexpand("kanji.greight.retard");
  build_kanji.road_enclose(build_kanji.level(
    kanji.radical.narrow_flag;
    build_kanji.sscale(xyscaled (0.76,0.65) shifted (40,-120))
      (kanji.grthree.sheep)));
  expand_pbox;
enddef;

vardef kanji.greight.turn_away =
  push_pbox_toexpand("kanji.greight.turn_away");
  build_kanji.road_enclose(kanji.greight.dismiss);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grnine.crawl =
  push_pbox_toexpand("kanji.grnine.crawl");
  build_kanji.road_enclose(kanji.grtwo.word);
  expand_pbox;
enddef;

vardef kanji.grnine.sojourn =
  push_pbox_toexpand("kanji.grnine.sojourn");
  build_kanji.road_enclose(build_kanji.sscale
    (scaled 0.94 shifted (0,-20-15*mincho))
    (kanji.grthree.bean));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% tai/shirizo "expel"
begintsuglyph("uni9000",0);
  kanji.grfive.expel;
  tsu_render;
endtsuglyph;

% sou/oku "send"
begintsuglyph("uni9001",1);
  kanji.grthree.send;
  tsu_render;
endtsuglyph;

% tou/ni "escape"
begintsuglyph("uni9003",3);
  kanji.greight.escape;
  tsu_render;
endtsuglyph;

% chiku "follow"
begintsuglyph("uni9010",16);
  kanji.greight.follow;
  tsu_render;
endtsuglyph;

% tou/todo "sojourn"
begintsuglyph("uni9017",23);
  kanji.grnine.sojourn;
  tsu_render;
endtsuglyph;

% sha/ha "crawl"
begintsuglyph("uni9019",25);
  kanji.grnine.crawl;
  tsu_render;
endtsuglyph;

% tsuu/too "distant"
begintsuglyph("uni901A",26);
  kanji.grtwo.traffic;
  tsu_render;
endtsuglyph;

% sei/yu "death"
begintsuglyph("uni901D",29);
  kanji.greight.death;
  tsu_render;
endtsuglyph;

% soku/haya "fast"
begintsuglyph("uni901F",31);
  kanji.grthree.fast;
  tsu_render;
endtsuglyph;

% zou/tsuku "make up"
begintsuglyph("uni9020",32);
  kanji.grfive.make_up;
  tsu_render;
endtsuglyph;

% ren/tsura "bring"
begintsuglyph("uni9023",35);
  kanji.grfour.bring;
  tsu_render;
endtsuglyph;

% shuu "week"
begintsuglyph("uni9031",49);
  kanji.grtwo.week;
  tsu_render;
endtsuglyph;

% sin/susu "proceed"
begintsuglyph("uni9032",50);
  kanji.grthree.proceed;
  tsu_render;
endtsuglyph;

% itsu/so "turn away"
begintsuglyph("uni9038",56);
  kanji.greight.turn_away;
  tsu_render;
endtsuglyph;

% chi/oku "retard"
begintsuglyph("uni9045",69);
  kanji.greight.retard;
  tsu_render;
endtsuglyph;

% aso "play"
begintsuglyph("uni904A",74);
  kanji.grthree.play;
  tsu_render;
endtsuglyph;

% sin/susu "proceed"
begintsuglyph("uni904B",75);
  kanji.grthree.fortune;
  tsu_render;
endtsuglyph;

% dou/michi "street"
begintsuglyph("uni9053",83);
  kanji.grtwo.street;
  tsu_render;
endtsuglyph;

% tatsu/tachi "suffix for pluralizing pronouns" (and several other meanings)
begintsuglyph("uni9054",84);
  kanji.grfour.plural;
  tsu_render;
endtsuglyph;

% son/shitaga "humble"
begintsuglyph("uni905C",92);
  kanji.greight.humble;
  tsu_render;
endtsuglyph;

% en/too "distant"
begintsuglyph("uni9060",96);
  kanji.grtwo.distant;
  tsu_render;
endtsuglyph;

% sen/era "choose"
begintsuglyph("uni9078",120);
  kanji.grfour.choose;
  tsu_render;
endtsuglyph;

% hi/sa "avoid"
begintsuglyph("uni907F",127);
  kanji.greight.avoid;
  tsu_render;
endtsuglyph;

% gen/koori "county"
begintsuglyph("uni90E1",225);
  kanji.grfour.county;
  tsu_render;
endtsuglyph;

% bu/be "section"
begintsuglyph("uni90E8",232);
  kanji.grthree.section;
  tsu_render;
endtsuglyph;

% to/miyako "metropolis"
begintsuglyph("uni90FD",253);
  kanji.grthree.metropolis;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
