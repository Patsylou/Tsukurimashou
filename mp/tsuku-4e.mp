%
% Unicode page 4E (Kanji) for Tsukurimashou
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

do_late_includes;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grthree.ride =
  push_pbox_toexpand("kanji.grthree.ride");
  build_kanji.add_beret(kanji.grone.wood);
  begingroup
    save myxf;
    transform myxf;
    (80,70) transformed myxf=(80,-30);
    (920,70) transformed myxf=(920,-30);
    (80,580) transformed myxf=(80,280);
    replace_strokep(-4)((point 0 of oldp)--(500,-70));
    replace_strokep(-2)(oldp transformed myxf);
    replace_strokep(-1)(oldp transformed myxf);
    push_stroke(get_strokep(-3) shifted (0,620-580),get_strokeq(-3));
    set_boserif(0,1,9);
    replace_strokep(-4)(oldp shifted (0,280-580));
    push_stroke((70,450)--(930,450),(1.6,1.6)--(1.6,1.6));
    set_boserif(0,1,9);
    push_stroke((260,620)--(260,280),(1.5,1.5)--(1.4,1.4));
    push_stroke((740,620)--(740,280),(1.5,1.5)--(1.4,1.4));
  endgroup;
  expand_pbox;
enddef;

vardef kanji.grthree.serve =
  push_pbox_toexpand("kanji.grthree.serve");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grfour.gentleman);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfour.go_between =
  push_pbox_toexpand("kanji.grfour.go_between");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.middle);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grfive.affair =
  push_pbox_toexpand("kanji.grfive.affair");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grtwo.cow);
  expand_pbox;
enddef;

vardef kanji.grfive.france =
  push_pbox_toexpand("kanji.grfive.france");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.radical.mu);
  expand_pbox;
enddef;

vardef kanji.grfive.long_time =
  push_pbox_toexpand("kanji.grfive.long_time");
  push_stroke((420,830)..tension 1.2..(270,570)..(120,460),
    (1.7,1.7)--(1.4,1.4)--(1,1));
  set_boserif(0,0,10);
  push_stroke((get_strokep(0) intersectionpoint ((0,690)--(1000,690)))--
      (690,690)..tension 1.2..(430,280)..(70,-10),
    (1.5,1.5)--(1.6,1.6)--(1.4,1.4)--(1.1,1.1));
  set_boserif(0,1,4);
  set_botip(0,1,0);
  push_stroke((point 1.60 of get_strokep(0))..tension 1.2..
      (710,160)..(900,0),
    (0.92,0.92)--(1.3,1.3)--(1.8,1.8));
  expand_pbox;
enddef;

vardef kanji.grfive.temporary =
  push_pbox_toexpand("kanji.grfive.temporary");
  build_kanji.lr(300,-10)
    (kanji.leftrad.person)
    (kanji.grthree.anti);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grsix.benevolence =
  push_pbox_toexpand("kanji.grsix.benevolence");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.two);
  expand_pbox;
enddef;

vardef kanji.grsix.riot =
  push_pbox_toexpand("kanji.grsix.riot");
  build_kanji.lr(540,-10)
    (tsu_xform(identity yscaled 0.94 shifted (0,-7))(kanji.grfive.tongue))
    (kanji.rare.mysterious);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.greight.completion =
  push_pbox_toexpand("kanji.greight.completion");
  kanji.grone.child;
  pop_stroke;
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grnine.offspring =
  push_pbox_toexpand("kanji.grnine.offspring");
  build_kanji.level(build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (build_kanji.sscale(shifted (0,-30) yscaled 0.95)
       (kanji.grone.child)));
  expand_pbox;
enddef;

vardef kanji.grnine.undergo =
  push_pbox_toexpand("kanji.grnine.undergo");
  build_kanji.level(build_kanji.tb(650,-20)
    (kanji.radical.kettle_lid)
    (build_kanji.tb(550,-40)
      (build_kanji.sscale(xscaled 0.9)(kanji.grone.mouth))
      (kanji.greight.completion)));
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.reference.enemy =
  push_pbox_toexpand("kanji.reference.enemy");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.nine);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.rare.surplus =
  push_pbox_toexpand("kanji.rare.surplus");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.power);
  expand_pbox;
enddef;

vardef kanji.rare.utensil =
  push_pbox_toexpand("kanji.rare.utensil");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.ten);
  expand_pbox;
enddef;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% ichi/hito "one"
begintsuglyph("uni4E00",0);
  kanji.grone.one;
  tsu_render;
endtsuglyph;

% chuu/hinoto: "street"; "counter for leaves or guns"; "fourth zodiac sign"
% Uwaa~!  Skala-sensei, I don't *want* to stick a thumbtack in my wrist!
begintsuglyph("uni4E01",1);
  kanji.grthree.thumbtack;
  tsu_render;
endtsuglyph;

% kou/samata "obstruction of breath"
begintsuglyph("uni4E02",2);
  kanji.rare.obstruction;
  tsu_render;
endtsuglyph;

% nana/shichi "seven"
begintsuglyph("uni4E03",3);
  kanji.grone.seven;
  tsu_render;
endtsuglyph;

% man/yorozu "myriad"
begintsuglyph("uni4E07",7);
  kanji.grtwo.myriad;
  tsu_render;
endtsuglyph;

% san/mi "three"
begintsuglyph("uni4E09",9);
  kanji.grone.three;
  tsu_render;
endtsuglyph;

% ue/jyou "up"
begintsuglyph("uni4E0A",10);
  kanji.grone.up;
  tsu_render;
endtsuglyph;

% shita/ka "down"
begintsuglyph("uni4E0B",11);
  kanji.grone.down;
  tsu_render;
endtsuglyph;

% fu "non-"
begintsuglyph("uni4E0D",13);
  kanji.grfour.non;
  tsu_render;
endtsuglyph;

% yo/ata "give"
begintsuglyph("uni4E0E",14);
  kanji.greight.give;
  tsu_render;
endtsuglyph;

% sho/ka "furthermore"
begintsuglyph("uni4E14",20);
  kanji.greight.furthermore;
  tsu_render;
endtsuglyph;

% hi/oo "glorious" (needed as part of other things)
begintsuglyph("uni4E15",21);
  kanji.rare.glorious;
  tsu_render;
endtsuglyph;

% sei/sou "world"
begintsuglyph("uni4E16",22);
  kanji.grthree.world;
  tsu_render;
endtsuglyph;

% kyuu/oka "hill"
begintsuglyph("uni4E18",24);
  kanji.greight.knoll;
  tsu_render;
endtsuglyph;

% hei/hinoe "third class or calendar sign"
begintsuglyph("uni4E19",25);
  kanji.greight.third;
  tsu_render;
endtsuglyph;

% ryo/teru "both," also a couple of different units of measure
begintsuglyph("uni4E21",33);
  kanji.grthree.both;
  tsu_render;
endtsuglyph;

% extra "vertical line"
begintsuglyph("uni4E28",40);
  kanji.radical.vline;
  tsu_render;
endtsuglyph;

% naka/chuu "middle"
begintsuglyph("uni4E2D",45);
  kanji.grone.middle;
  tsu_render;
endtsuglyph;

% maru "circle"
begintsuglyph("uni4E38",56);
  kanji.grtwo.circle;
  tsu_render;
endtsuglyph;

% shu/nushi "master"
begintsuglyph("uni4E3B",59);
  kanji.grthree.master;
  tsu_render;
endtsuglyph;

% kyuu/hisa "long time" (as in "hisashiburi")
begintsuglyph("uni4E45",69);
  kanji.grfive.long_time;
  tsu_render;
endtsuglyph;

% diagonal stroke (like katakana "no") for use in decomposition dictionary
begintsuglyph("uni4E3F",63);
  push_stroke((630,770)..tension 1.2..(440,260)..(120,-50),
    (1.6,1.6)--(1.4,1.4)--(0.9,0.9));
  set_boserif(0,0,10);
  tsu_render;
endtsuglyph;

% sa/naga "notwithstanding" (needed for tsuku "make")
begintsuglyph("uni4E4D",77);
  kanji.rare.notwithstanding;
  tsu_render;
endtsuglyph;

% shou/no "ride"
begintsuglyph("uni4E57",87);
  kanji.grthree.ride;
  tsu_render;
endtsuglyph;

% extra (needed as component)
begintsuglyph("uni4E5A",90);
  kanji.rare.mysterious;
  tsu_render;
endtsuglyph;

% kyuu/kokono "nine"
begintsuglyph("uni4E5D",93);
  kanji.grone.nine;
  tsu_render;
endtsuglyph;

% ya/nari "to be" (archaic)
begintsuglyph("uni4E5F",95);
  kanji.grnine.to_be;
  tsu_render;
endtsuglyph;

% ran/mida "riot"
begintsuglyph("uni4E71",113);
  kanji.grsix.riot;
  tsu_render;
endtsuglyph;

% ryou "completion"
begintsuglyph("uni4E86",134);
  kanji.greight.completion;
  tsu_render;
endtsuglyph;

% yo/arakaji "before"
begintsuglyph("uni4E88",136);
  kanji.grthree.before;
  tsu_render;
endtsuglyph;

% sou/ika "dispute"
begintsuglyph("uni4E89",137);
  kanji.grfour.dispute;
  tsu_render;
endtsuglyph;

% ji/koto "thing"
begintsuglyph("uni4E8B",139);
  kanji.grthree.thing;
  tsu_render;
endtsuglyph;

% ni/futa "two"
begintsuglyph("uni4E8C",140);
  kanji.grone.two;
  tsu_render;
endtsuglyph;

% u "deficient"
begintsuglyph("uni4E8F",143);
  kanji.rare.deficient;
  tsu_render;
endtsuglyph;

% un/i "say"
begintsuglyph("uni4E91",145);
  kanji.grnine.declaim;
  tsu_render;
endtsuglyph;

% go/itsu "five"
begintsuglyph("uni4E94",148);
  kanji.grone.five;
  tsu_render;
endtsuglyph;

% sei/i "well" (water supply)
begintsuglyph("uni4E95",149);
  kanji.greight.well;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4E96",150);
  build_kanji.tb(430,-50)
    (kanji.grone.two)
    (kanji.grone.two);
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4E97",151);
  build_kanji.tb(450,-50)
    (kanji.grone.mountain)
    (kanji.grone.two);
  tsu_render;
endtsuglyph;

% a/tsu "sub-" (also, "-ous, lower oxidation state")
begintsuglyph("uni4E9C",156);
  kanji.greight.prefix_sub;
  tsu_render;
endtsuglyph;

% not in dictionaries but needed for grfour.extremely
begintsuglyph("uni4E9F",159);
  kanji.rare.extremely;
  tsu_render;
endtsuglyph;

% "kettle lid radical"
begintsuglyph("uni4EA0",160);
  build_kanji.tb(500,-50)
    (kanji.radical.kettle_lid)
    ();
  tsu_render;
endtsuglyph;

% bou/na "dead"
begintsuglyph("uni4EA1",161);
  kanji.grsix.dead;
  tsu_render;
endtsuglyph;

% kou/takaburu "high spirits"
begintsuglyph("uni4EA2",162);
  kanji.rare.high_spirits;
  tsu_render;
endtsuglyph;

% kou/maji "mix"
begintsuglyph("uni4EA4",164);
  kanji.grtwo.mix;
  tsu_render;
endtsuglyph;

% eki/mata "again"
begintsuglyph("uni4EA6",166);
  kanji.grnine.again;
  tsu_render;
endtsuglyph;

% kou "undergo" (rarer form)
begintsuglyph("uni4EA8",168);
  kanji.grnine.undergo;
  tsu_render;
endtsuglyph;

% kyou "undergo" (more common form)
begintsuglyph("uni4EAB",171);
  kanji.greight.undergo;
  tsu_render;
endtsuglyph;

% kyou "capital"
begintsuglyph("uni4EAC",172);
  kanji.grtwo.capital;
  tsu_render;
endtsuglyph;

% tei "head of household"
begintsuglyph("uni4EAD",173);
  kanji.greight.head_of_household;
  tsu_render;
endtsuglyph;


% extra "capital"
begintsuglyph("uni4EB0",176);
  build_kanji.level(build_kanji.tb(650,-40)
    (kanji.radical.kettle_lid)
    (build_kanji.tb(500,70)
      (build_kanji.sscale(xscaled 0.9)(kanji.grone.day))
      (kanji.grone.small)));
  set_boserif(-1,0,whatever);
  tsu_render;
endtsuglyph;

% hito/jin "person"
begintsuglyph("uni4EBA",186);
  kanji.grone.person;
  tsu_render;
endtsuglyph;

% hito/jin "person" as radical
begintsuglyph("uni4EBB",187);
  build_kanji.sscale(xscaled 0.5)(kanji.leftrad.person);
  tsu_render;
endtsuglyph;

% juu "utensil"
begintsuglyph("uni4EC0",192);
  kanji.rare.utensil;
  tsu_render;
endtsuglyph;

% jin "benevolence"
begintsuglyph("uni4EC1",193);
  kanji.grsix.benevolence;
  tsu_render;
endtsuglyph;

% "surplus or excess"
begintsuglyph("uni4EC2",194);
  kanji.rare.surplus;
  tsu_render;
endtsuglyph;

% ada "enemy"
begintsuglyph("uni4EC7",199);
  kanji.reference.enemy;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4EC8",200);
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.eight);
  tsu_render;
endtsuglyph;

% kon/ima "now"
begintsuglyph("uni4ECA",202);
  kanji.grtwo.now;
  tsu_render;
endtsuglyph;

% kai "mediate"
begintsuglyph("uni4ECB",203);
  kanji.greight.mediate;
  tsu_render;
endtsuglyph;

% futsu/hotoke "France" or "Buddha"
begintsuglyph("uni4ECF",207);
  kanji.grfive.france;
  tsu_render;
endtsuglyph;

% ko "offspring"
begintsuglyph("uni4ED4",212);
  kanji.grnine.offspring;
  tsu_render;
endtsuglyph;

% shi/tsuka "serve"
begintsuglyph("uni4ED5",213);
  kanji.grthree.serve;
  tsu_render;
endtsuglyph;

% ta/hoka "other"
begintsuglyph("uni4ED6",214);
  kanji.grthree.other;
  tsu_render;
endtsuglyph;

% fu/tsu "attach"
begintsuglyph("uni4ED8",216);
  kanji.grfour.attach;
  tsu_render;
endtsuglyph;

% sen "wizard"
begintsuglyph("uni4ED9",217);
  kanji.greight.wizard;
  tsu_render;
endtsuglyph;

% dai/kawa "generation"
begintsuglyph("uni4EE3",227);
  kanji.grthree.generation;
  tsu_render;
endtsuglyph;

% rei "orders" (but usually part of honorific titles for family members)
begintsuglyph("uni4EE4",228);
  kanji.grfour.orders;
  tsu_render;
endtsuglyph;

% i/mo(tsu) "because" or "with regard to"
begintsuglyph("uni4EE5",229);
  kanji.grfour.as;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4EE8",232);
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (build_kanji.sscale(shifted (0,-30) yscaled 0.95)
       (kanji.grone.three));
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4EE9",233);
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grone.up);
  tsu_render;
endtsuglyph;

% ka/kari "temporary"
begintsuglyph("uni4EEE",238);
  kanji.grfive.temporary;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4EEF",239);
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grtwo.little);
  tsu_render;
endtsuglyph;

% naka/chuu "go-between"
begintsuglyph("uni4EF2",242);
  kanji.grfour.go_between;
  tsu_render;
endtsuglyph;

% extra
begintsuglyph("uni4EF4",244);
  build_kanji.lr(300,-20)
    (kanji.leftrad.person)
    (kanji.grone.moon);
  tsu_render;
endtsuglyph;

% ken/kudan "affair"
begintsuglyph("uni4EF6",246);
  kanji.grfive.affair;
  tsu_render;
endtsuglyph;

% nin/maka "responsibility"
begintsuglyph("uni4EFB",251);
  kanji.grfive.responsibility;
  tsu_render;
endtsuglyph;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

endfont;

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
