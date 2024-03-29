%
% Unicode page 5B (Kanji) for Tsukurimashou
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
input toprad.mp;

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grone.study =
  push_pbox_toexpand("kanji.grone.study");
  begingroup
    save saved_path;
    path saved_path;
    build_kanji.tb(740,100)
      (kanji.toprad.alt_sparkle_three)
      (build_kanji.tb(580,150)
         (kanji.radical.conservative_hat)
         (build_kanji.sscale(xscaled 0.75)
            (kanji.grone.child;saved_path:=get_strokep(0));
             replace_strokep(0)(saved_path)));
  endgroup;
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grtwo.chamber =
  push_pbox_toexpand("kanji.grtwo.chamber");
  build_kanji.tb(600,150)
    (kanji.radical.silly_hat)
    (build_kanji.level(build_kanji.tb(420,100)
       (build_kanji.sscale(scaled 0.7)(kanji.radical.mu_bar))
       (kanji.grone.earth)));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grthree.counterpart =
  push_pbox_toexpand("kanji.grthree.counterpart");
  build_kanji.lr(420,0)
    (kanji.leftrad.sentence)
    (kanji.grsix.inch);
  expand_pbox;
enddef;

vardef kanji.grthree.cold =
  push_pbox_toexpand("kanji.grthree.cold");
  build_kanji.level(build_kanji.tb(640,150)
    (kanji.radical.silly_hat)
    (build_kanji.sscale(yscaled 0.7 shifted (0,100))
       (build_kanji.tb(290,46)
          (build_kanji.sscale(xscaled 0.8)(kanji.toprad.grass);
           replace_strokep(0)(oldp shifted (0,140));
           build_kanji.sscale(xscaled 0.77 shifted (0,-130))(kanji.grone.one);
           tsu_xform(identity shifted (0,-420))(kanji.grone.one))
          (tsu_xform(identity shifted (20,0))(kanji.radical.eight)));
     push_stroke((430,260)..tension 1.2..(570,220)..(660,180),
       (1.2,1.2)--(1.5,1.5)--(1.8,1.8));
     push_stroke((300,130)..tension 1.2..(580,40)..(690,-10),
       (1.2,1.2)--(1.5,1.5)--(1.8,1.8))));
  expand_pbox;
enddef;

vardef kanji.grthree.inn =
  push_pbox_toexpand("kanji.grthree.protect");
  build_kanji.tb(600,110)
    (kanji.radical.silly_hat)
    (kanji.rare.centurion);
  expand_pbox;
enddef;

vardef kanji.grthree.palace =
  push_pbox_toexpand("kanji.grthree.palace");
  build_kanji.tb(600,90)
    (kanji.radical.silly_hat)
    (kanji.greight.spine);
  expand_pbox;
enddef;

vardef kanji.grthree.protect =
  push_pbox_toexpand("kanji.grthree.protect");
  build_kanji.tb(600,210)
    (kanji.radical.silly_hat)
    (kanji.grsix.inch);
  expand_pbox;
enddef;

vardef kanji.grthree.reality =
  push_pbox_toexpand("kanji.grthree.reality");
  build_kanji.tb(600,170-50*mincho)
    (kanji.radical.silly_hat)
    (kanji.grfour.husband;
     replace_strokep(-3)(oldp shifted (0,60));
     push_stroke((130,510)--(870,510),
       (1.55,1.55)--(1.55,1.55));
     set_boserif(0,1,9));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.guess =
  push_pbox_toexpand("kanji.grfour.guess");
  build_kanji.tb(600,70)
    (kanji.radical.silly_hat)
    (kanji.grthree.festival);
  expand_pbox;
enddef;

vardef kanji.grfour.injury =
  push_pbox_toexpand("kanji.grfour.injury");
  build_kanji.level(build_kanji.tb(640,200)
    (kanji.radical.silly_hat)
    (build_kanji.sscale(xscaled 0.84)(build_kanji.tb(420,0)
       (kanji.toprad.blue)
       (kanji.grone.mouth))));
  expand_pbox;
enddef;

vardef kanji.grfour.season =
  push_pbox_toexpand("kanji.grfour.season");
  build_kanji.tb(340,100)
    (kanji.grnine.two_branch_tree;
     replace_strokep(-4)(subpath (0,0.78) of oldp))
    (path charsvp;
     build_kanji.sscale(xscaled 0.75)
       (kanji.grone.child;
        charsvp:=get_strokep(0));
     replace_strokep(0)(charsvp));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfive.draw_near =
  push_pbox_toexpand("kanji.grfive.draw_near");
  build_kanji.tb(600,180)
    (kanji.radical.silly_hat)
    (build_kanji.sscale(xscaled (1-0.15*mincho))(kanji.greight.strange));
  expand_pbox;
enddef;

vardef kanji.grfive.wealth =
  push_pbox_toexpand("kanji.grfive.wealth");
  build_kanji.tb(600,200)
    (kanji.radical.silly_hat)
    (kanji.rare.roll_of_cloth;
     replace_strokep(-6)(oldp shifted (-500,0) xscaled (0.8-0.1*mincho)
                         shifted (500,0)));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grsix.filial_piety =
  push_pbox_toexpand("kanji.grsix.filial_piety");
  kanji.radical.old_man(kanji.grone.child);
  expand_pbox;
enddef;

vardef kanji.grsix.religion =
  push_pbox_toexpand("kanji.grsix.religion");
  build_kanji.tb(600,100)
    (kanji.radical.silly_hat)
    (build_kanji.sscale(xscaled 0.94)(kanji.grfive.showing));
  expand_pbox;
enddef;

vardef kanji.grsix.space =
  push_pbox_toexpand("kanji.grsix.space");
  build_kanji.tb(570,150)
    (kanji.radical.silly_hat)
    (kanji.grthree.reason);
  expand_pbox;
enddef;

vardef kanji.grsix.treasure =
  push_pbox_toexpand("kanji.grsix.treasure");
  build_kanji.tb(600,150)
    (kanji.radical.silly_hat)
    (kanji.grone.ball;
     replace_strokep(-4)(oldp shifted (-500,0) xscaled (0.8-0.1*mincho)
                         shifted (500,0)));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grnine.cave =
  push_pbox_toexpand("kanji.grnine.cave");
  build_kanji.tb(620,150)
    (kanji.radical.silly_hat)
    (kanji.grone.right);
  expand_pbox;
enddef;

vardef kanji.grnine.song_dynasty =
  push_pbox_toexpand("kanji.grnine.song_dynasty");
  build_kanji.tb(600,180)
    (kanji.radical.silly_hat)
    (kanji.grone.wood);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% ko "child"
begintsuglyph("uni5B50",80);
  kanji.grone.child;
  tsu_render;
endtsuglyph;

% ketsu/hitori "mosquito larva"
begintsuglyph("uni5B51",81);
  kanji.rare.mosquito_larva;
  tsu_render;
endtsuglyph;

% ji "character"
begintsuglyph("uni5B57",87);
  kanji.grone.character;
  tsu_render;
endtsuglyph;

% extra: fu/tamago "enfold"
begintsuglyph("uni5B5A",90);
  build_kanji.level(build_kanji.tb(580,0)
    (kanji.toprad.paw)
    (kanji.grone.child));
  tsu_render;
endtsuglyph;

% kou/taka "filial piety"
begintsuglyph("uni5B5D",93);
  kanji.grsix.filial_piety;
  tsu_render;
endtsuglyph;

% ki "season"
begintsuglyph("uni5B63",99);
  kanji.grfour.season;
  tsu_render;
endtsuglyph;

% gaku "school"
begintsuglyph("uni5B66",102);
  kanji.grone.study;
  tsu_render;
endtsuglyph;

% son/mago "grandchild"
begintsuglyph("uni5B6B",107);
  kanji.grfour.grandchild;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni5B71",113);
  build_kanji.level(
    kanji.radical.narrow_flag;
    build_kanji.sscale(xyscaled (0.73,0.67) shifted (40,-150))
      (build_kanji.tricluster(0.7)
         (kanji.grone.child)
         (build_kanji.sscale(scaled 0.9)(kanji.grone.child))
         (build_kanji.sscale(scaled 0.9)(kanji.grone.child))));
  tsu_render;
endtsuglyph;

% silly hat radical
begintsuglyph("uni5B80",128);
  build_kanji.tb(500,-30)
    (kanji.radical.silly_hat)
    ();
  tsu_render;
endtsuglyph;

% cho/takuwaeru "bobbin"
begintsuglyph("uni5B81",129);
  kanji.rare.bobbin;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni5B82",130);
  build_kanji.tb(550,180)
    (kanji.radical.silly_hat)
    (kanji.radical.legs);
  tsu_render;
endtsuglyph;

% ta/hebi "other"
begintsuglyph("uni5B83",131);
  kanji.rare.other;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni5B84",132);
  build_kanji.tb(600,220)
    (kanji.radical.silly_hat)
    (build_kanji.sscale(xscaled 0.9 shifted (80-30*mincho,0))
       (kanji.grone.nine));
  tsu_render;
endtsuglyph;

% shu/mamo "protect"
begintsuglyph("uni5B88",136);
  kanji.grthree.protect;
  tsu_render;
endtsuglyph;

% an/yasu "easy/cheap"
begintsuglyph("uni5B89",137);
  kanji.grthree.easy;
  tsu_render;
endtsuglyph;

% "song dynasty"
begintsuglyph("uni5B8B",139);
  kanji.grnine.song_dynasty;
  tsu_render;
endtsuglyph;

% kan "perfect"
begintsuglyph("uni5B8C",140);
  kanji.grfour.perfect;
  tsu_render;
endtsuglyph;

% extra "meat"
begintsuglyph("uni5B8D",141);
  build_kanji.tb(550,100)
    (kanji.radical.silly_hat)
    (kanji.grone.six);
  tsu_render;
endtsuglyph;

% "cave"
begintsuglyph("uni5B95",149);
  kanji.grnine.cave;
  tsu_render;
endtsuglyph;

% shuu/mune "religion"
begintsuglyph("uni5B97",151);
  kanji.grsix.religion;
  tsu_render;
endtsuglyph;

% kan "bureaucracy"
begintsuglyph("uni5B98",152);
  kanji.grfour.bureaucracy;
  tsu_render;
endtsuglyph;

% chuu "space"
begintsuglyph("uni5B99",153);
  kanji.grsix.space;
  tsu_render;
endtsuglyph;

% tei/sada "regular"
begintsuglyph("uni5B9A",154);
  kanji.grthree.regular;
  tsu_render;
endtsuglyph;

% hou/takara "treasure"
begintsuglyph("uni5B9D",157);
  kanji.grsix.treasure;
  tsu_render;
endtsuglyph;

% jitsu/mi "reality"
begintsuglyph("uni5B9F",159);
  kanji.grthree.reality;
  tsu_render;
endtsuglyph;

% kyaku "guest"
begintsuglyph("uni5BA2",162);
  kanji.grthree.guest;
  tsu_render;
endtsuglyph;

% shitsu/muro "chamber"
begintsuglyph("uni5BA4",164);
  kanji.grtwo.chamber;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni5BAA",170);
  build_kanji.tb(600,160)
    (kanji.radical.silly_hat)
    (kanji.grone.ahead);
  tsu_render;
endtsuglyph;

% kyuu/miya "house"
begintsuglyph("uni5BAE",174);
  kanji.grthree.palace;
  tsu_render;
endtsuglyph;

% gai "injury"
begintsuglyph("uni5BB3",179);
  kanji.grfour.injury;
  tsu_render;
endtsuglyph;

% ka/ie "house"
begintsuglyph("uni5BB6",182);
  kanji.grtwo.house;
  tsu_render;
endtsuglyph;

% you/i "face"
begintsuglyph("uni5BB9",185);
  kanji.grfive.face;
  tsu_render;
endtsuglyph;

% shuku/yado "inn"
begintsuglyph("uni5BBF",191);
  kanji.grthree.inn;
  tsu_render;
endtsuglyph;

% ki/yo "draw near"
begintsuglyph("uni5BC4",196);
  kanji.grfive.draw_near;
  tsu_render;
endtsuglyph;

% fu/to "wealth"
begintsuglyph("uni5BCC",204);
  kanji.grfive.wealth;
  tsu_render;
endtsuglyph;

% kan/samu "cold"
begintsuglyph("uni5BD2",210);
  kanji.grthree.cold;
  tsu_render;
endtsuglyph;

% satsu "guess"
begintsuglyph("uni5BDF",223);
  kanji.grfour.guess;
  tsu_render;
endtsuglyph;

% su/miki "inch", also often a radical
begintsuglyph("uni5BF8",248);
  kanji.grsix.inch;
  tsu_render;
endtsuglyph;

% tera/ji "temple"
begintsuglyph("uni5BFA",250);
  kanji.grtwo.temple;
  tsu_render;
endtsuglyph;

% tai/aite "counterpart"
begintsuglyph("uni5BFE",254);
  kanji.grthree.counterpart;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
