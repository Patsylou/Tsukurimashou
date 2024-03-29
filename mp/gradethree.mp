%
% Grade Three kanji used on more than one page
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

inclusion_lock(gradethree);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

vardef kanji.grthree.accept =
  push_pbox_toexpand("kanji.grthree.accept");
  build_kanji.tb(640,80)
    (kanji.toprad.paw)
    (build_kanji.tb(540,70)
       (kanji.radical.conservative_hat)
       (kanji.greight.otoh));
  expand_pbox;
enddef;

vardef kanji.grthree.anti =
  push_pbox_toexpand("kanji.grthree.anti");
  build_kanji.cliff_enclose
    (tsu_xform(identity xyscaled (1.09,0.9) shifted (-80,-30))
       (kanji.greight.otoh));
  expand_pbox;
enddef;

vardef kanji.grthree.bean =
  push_pbox_toexpand("kanji.grthree.bean");
  push_stroke((120,810)--(880,810),
    (1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  build_kanji.box((180,650),(840,400));
  push_stroke((250,280)..tension 1.2..(350,140)..(380,60),
    (1.2,1.2)--(1.4,1.4)--(1.7,1.7));
  push_stroke((750,290)..tension 1.2..(660,130)..(580,40),
    (1.6,1.6)--(1.5,1.5)--(1.3,1.3));
  set_boserif(0,0,10);
  push_stroke((120,0)--(880,0),
    (1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  expand_pbox;
enddef;

vardef kanji.grthree.before =
  push_pbox_toexpand("kanji.grthree.before");
  push_stroke((260,780)--(710,780)..(610,650)..(530,600),
    (1.5,1.5)--(1.6,1.6)--(1.4,1.4)--(1,1));
  set_boserif(0,1,4);
  set_botip(0,1,0);
  push_stroke((410,660)..tension 1.2..(530,600)..(590,530),
    (1,1)--(1.4,1.4)--(1.8,1.8));
  push_stroke((130,480)--(870,480)..(770,350)..(690,300),
    (1.6,1.6)--(1.6,1.6)--(1.4,1.4)--(1,1));
  set_boserif(0,1,4);
  set_botip(0,1,0);
  push_stroke((500,480),
    (1.6,1.6));
  build_kanji.add_jtail(0);
  expand_pbox;
enddef;

vardef kanji.grthree.blessing =
  push_pbox_toexpand("kanji.grthree.blessing");
  build_kanji.level(build_kanji.tb(480,0)
    (kanji.grone.earth)
    (build_kanji.sscale(xyscaled (1.2,1.4) shifted (0,-140))
       (kanji.grthree.sheep);
     obstacktype[find_stroke(-3)]:=otnull;
     replace_strokep(0)((point 0 of oldp)--(500,-50))));
  expand_pbox;
enddef;

vardef kanji.grthree.both =
  push_pbox_toexpand("kanji.grthree.both");
  build_kanji.level(
    build_kanji.tb(600,0)
      (kanji.grone.one)
      (kanji.radical.gmm;
       build_kanji.sscale(scaled 0.45)(kanji.radical.open_box));
    push_stroke(
      (point 0.5 of get_strokep(-4))--(point 0.5 of get_strokep(-1)),
      (1.7,1.7)--(1.5,1.5)));
  expand_pbox;
enddef;

vardef kanji.grthree.centre =
  push_pbox_toexpand("kanji.grthree.centre");
  kanji.grfour.husband;
  replace_strokep(-3)((240,360)--(240,610)--(760,610)--(760,360));
  replace_strokeq(-3)((1.4,1.4)--(1.5,1.5)--(1.5,1.5)--(1.4,1.4));
  set_boserif(-3,1,4);
  set_boserif(-3,2,4);
  set_botip(-3,1,1);
  set_botip(-3,2,1);
  expand_pbox;
enddef;

vardef kanji.grthree.change =
  push_pbox_toexpand("kanji.grthree.change");
  build_kanji.level(build_kanji.lr(470,-100)
    (tsu_xform(identity yscaled 0.98)(kanji.leftrad.person))
    (kanji.radical.spoon));
  set_boserif(0,0,10);
  expand_pbox;
enddef;

vardef kanji.grthree.chapter =
  push_pbox_toexpand("kanji.grthree.chapter");
  build_kanji.level(build_kanji.tb(480,-70)
    (kanji.toprad.stand_up)
    (kanji.grone.early));
  expand_pbox;
enddef;

vardef kanji.grthree.climb =
  push_pbox_toexpand("kanji.grthree.climb");
  kanji.radical.dotted_tent;
  tsu_xform(identity shifted (-500,50) xyscaled (0.7,0.54) shifted (500,-50))
    (kanji.grthree.bean);
  obstacktype[find_whatever(otstroke,-5)]:=otnull;
  replace_strokep(0)(oldp shifted (-500,0) xscaled 1.2 shifted (500,0));
  expand_pbox;
enddef;

vardef kanji.grthree.collect =
  push_pbox_toexpand("kanji.grthree.collect");
  build_kanji.tb(340,60)
    (build_kanji.sscale(shifted (-10,0))(kanji.radical.old_bird))
    (build_kanji.sscale(shifted (20,0))(kanji.grone.wood));
  expand_pbox;
enddef;

vardef kanji.grthree.column =
  push_pbox_toexpand("kanji.grthree.column");
  build_kanji.level(build_kanji.lr(560,20)
    (build_kanji.sscale(yscaled 0.92)(kanji.leftrad.bare_bone))
    (kanji.rightrad.sword));
  expand_pbox;
enddef;

vardef kanji.grthree.commerce =
  push_pbox_toexpand("kanji.grthree.commerce");
  build_kanji.level(build_kanji.tb(620,60)
    (kanji.toprad.stand_up;
     pop_stroke)
    (kanji.rare.clear));
  expand_pbox;
enddef;

vardef kanji.grthree.complete =
  push_pbox_toexpand("kanji.grthree.complete");
  kanji.radical.tent;
  pop_stroke;
  tsu_xform(identity yscaled 0.56)(kanji.grone.king);
  replace_strokep(-3)((220,ypart point 0 of oldp)--
    (770,ypart point 1 of oldp));
  expand_pbox;
enddef;

vardef kanji.grthree.correlation =
  push_pbox_toexpand("kanji.grthree.correlation");
  build_kanji.level(build_kanji.lr(440,80)
    (kanji.leftrad.wood)
    (kanji.grone.eye));
  expand_pbox;
enddef;

vardef kanji.grthree.dish =
  push_pbox_toexpand("kanji.grthree.dish");
  push_stroke((200,0)--(200,760)--(800,760)--(800,0),
    (1.5,1.5)--(1.7,1.7)--(1.7,1.7)--(1.5,1.5));
  set_botip(0,1,1);
  set_botip(0,2,1);
  set_boserif(0,1,4);
  set_boserif(0,2,4);
  push_stroke((400,760)--(400,0),(1.6,1.6)--(1.5,1.5));
  push_stroke((600,760)--(600,0),(1.6,1.6)--(1.5,1.5));
  push_stroke((90,0)--(910,0),(1.5,1.5)--(1.5,1.5));
  set_boserif(0,1,9);
  expand_pbox;
enddef;

vardef kanji.grthree.easy =
  push_pbox_toexpand("kanji.grthree.easy");
  build_kanji.tb(600,150)
    (kanji.radical.silly_hat)
    (kanji.grone.woman);
  expand_pbox;
enddef;

vardef kanji.grthree.existence =
  push_pbox_toexpand("kanji.grthree.existence");
  build_kanji.lean_to((430,240));
  build_kanji.sscale(xyscaled (0.76,0.63) shifted (30,-170))
    (kanji.bottomrad.moon);
  expand_pbox;  
enddef;

vardef kanji.grthree.face =
  push_pbox_toexpand("kanji.grthree.face");
  push_stroke((80,760)--(920,760),
    (1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  push_stroke((500,760)..(480,660)..(450,560),
    (1.7,1.7)--(1.3,1.3)--(1,1));
  build_kanji.box((150,560),(850,0));
  push_stroke((370,560)--(370,0),(1.6,1.6)--(1.5,1.5));
  push_stroke((630,560)--(630,0),(1.6,1.6)--(1.5,1.5));
  push_stroke((370,390)--(630,390),(1.5,1.5)--(1.5,1.5));
  push_stroke((370,200)--(630,200),(1.5,1.5)--(1.5,1.5));
  for i=-7 upto 0: set_bosize(i,92); endfor;
  expand_pbox;
enddef;

vardef kanji.grthree.farming =
  push_pbox_toexpand("kanji.grthree.farming");
  build_kanji.level(build_kanji.tb(480,-20)
    (kanji.grthree.melody)
    (kanji.grnine.dragon_sign));
  expand_pbox;
enddef;

vardef kanji.grthree.festival =
  push_pbox_toexpand("kanji.grthree.festival");
  build_kanji.level(
    push_stroke((230,810)..tension 1.2..(170,670)..(50,540),
      (1.5,1.5)--(1.3,1.3)--(0.85,0.9));
    set_boserif(0,0,10);
    push_stroke((240,780)--(460,780)..(410,680)..(300,540)..(60,370),
      (1.5,1.5)--(1.6,1.6)--(1.5,1.5)--(1.3,1.3)--(0.85,0.9));
    set_boserif(0,1,4);
    set_botip(0,1,0);
    push_stroke((210,700)..tension 1.2..(270,670)..(320,620),
      (1,1)--(1.4,1.4)--(1.7,1.7));
    push_stroke((140,590)..tension 1.2..(200,560)..(250,510),
      (1,1)--(1.4,1.4)--(1.7,1.7));
    push_stroke((540,810)..tension 1.2..(680,570)..(920,400),
      (1,1)--(1.3,1.3)--(2.25,0.95));
    push_stroke((575,780)--(880,780)..(830,700)..(780,650)..(670,580),
      (1.5,1.5)--(1.6,1.6)--(1.5,1.5)--(1.3,1.3)--(0.85,0.9));
    set_boserif(0,1,4);
    set_botip(0,1,0);
    push_stroke((330,470)--(660,470),(1.6,1.6)--(1.6,1.6));
    set_boserif(0,1,9);
    build_kanji.sscale(yscaled 0.6 shifted (0,-150))(kanji.grfive.showing);
    obstacktype[find_whatever(otstroke,-4)]:=otnull;
  );
  expand_pbox;
enddef;

vardef kanji.grthree.formula =
  push_pbox_toexpand("kanji.grthree.formula");
  kanji.radical.ceremony;
  replace_strokep(-1)(oldp shifted (-950,0) xscaled 0.6 shifted (950,0));
  replace_strokep(0)(oldp shifted (-950,0) xscaled 0.6 shifted (950,0));
  build_kanji.sscale(xyscaled (0.6,0.7) shifted (-150,-200))
    (build_kanji.lift_skirt(400,250)(kanji.grtwo.craft));
  expand_pbox;
enddef;

vardef kanji.grthree.generation =
  push_pbox_toexpand("kanji.grthree.generation");
  build_kanji.lr(300,30)
    (kanji.leftrad.person)
    (kanji.radical.ceremony);
  expand_pbox;
enddef;

vardef kanji.grthree.go_away =
  push_pbox_toexpand("kanji.grthree.go_away");
  build_kanji.tb(340,20)
    (kanji.grone.earth)
    (kanji.radical.mu);
  expand_pbox;
enddef;

vardef kanji.grthree.goods =
  push_pbox_toexpand("kanji.grthree.goods");
  build_kanji.level(build_kanji.tricluster(0.7)
    (kanji.grone.mouth)
    (kanji.grone.mouth)
    (kanji.grone.mouth));
  expand_pbox;
enddef;

vardef kanji.grthree.guest =
  push_pbox_toexpand("kanji.grthree.guest");
  build_kanji.tb(600,110)
    (kanji.radical.silly_hat)
    (kanji.grfour.every);
  expand_pbox;
enddef;

vardef kanji.grthree.have_honour =
  push_pbox_toexpand("kanji.grthree.have_honour");
  build_kanji.level(
    build_kanji.sscale(yscaled 0.52 shifted (0,30))(kanji.grone.day);
    build_kanji.sscale(yscaled 1.1)(kanji.radical.vline));
  expand_pbox;
enddef;

vardef kanji.grthree.heap_big =
  push_pbox_toexpand("kanji.grthree.heap_big");
  build_kanji.add_beret(kanji.grone.wheel);
  push_stroke(get_strokep(-1),(1.6,1.6)--(1.6,1.6));
  replace_strokep(0)(oldp shifted (0,-50-ypart llcorner oldp));
  replace_strokep(-2)
    ((160,ypart point 0 of oldp)--(840,ypart point 1 of oldp));
  set_boserif(0,1,9);
  set_bosize(0,get_bosize(-2));
  expand_pbox;
enddef;

vardef kanji.grthree.hurry =
  push_pbox_toexpand("kanji.grthree.hurry");
  build_kanji.level(build_kanji.tb(390,40)
    (build_kanji.sscale(xscaled 0.9 shifted (40,0))
       (build_kanji.attach_fishhook(0.6)
          (kanji.radical.pigs_head)))
    (kanji.bottomrad.heart));
  expand_pbox;
enddef;

vardef kanji.grthree.ingredient =
  push_pbox_toexpand("kanji.grthree.ingredient");
  build_kanji.level(build_kanji.tb(260,-50)
    (kanji.grone.eye)
    (build_kanji.tb(600,0)
       (kanji.grone.one)
       (kanji.radical.eight)));
  expand_pbox;
enddef;

vardef kanji.grthree.intention =
  push_pbox_toexpand("kanji.grthree.intention");
  build_kanji.level(build_kanji.tb(210,0)
    (kanji.grone.noise)
    (kanji.bottomrad.heart));
  expand_pbox;
enddef;

vardef kanji.grthree.junior =
  push_pbox_toexpand("kanji.grthree.junior");
  kanji.rare.director;
  build_kanji.box((320,240),(810,0));
  expand_pbox;
enddef;

vardef kanji.grthree.learn =
  push_pbox_toexpand("kanji.grthree.learn");
  build_kanji.level(build_kanji.tb(400,20)
    (kanji.grtwo.feather)
    (tsu_xform(identity shifted (40,0))(kanji.grone.white)));
  expand_pbox;
enddef;

vardef kanji.grthree.level =
  push_pbox_toexpand("kanji.grthree.level");
  build_kanji.level(
    kanji.grsix.dry;
    replace_strokep(-1)(oldp shifted (0,-90));
    build_kanji.sscale(xyscaled (0.8,0.52) shifted (0,190))
      (kanji.toprad.sparkle_two));
  expand_pbox;
enddef;

vardef kanji.grthree.master =
  push_pbox_toexpand("kanji.grthree.master");
  build_kanji.level(
    tsu_xform(identity yscaled (590/710))(kanji.grone.king);
    push_stroke((400,810)..(480,750)..(560,670),
      (1,1)--(1.4,1.4)--(1.8,1.8)));
  expand_pbox;
enddef;

vardef kanji.grthree.melody =
  push_pbox_toexpand("kanji.grthree.melody");
  build_kanji.box((140,600),(860,30));
  push_stroke((380,810)--(380,40),
    (1.7,1.7)--(1.5,1.5));
  set_boserif(0,0,10);
  push_stroke((620,810)--(620,40),
    (1.7,1.7)--(1.5,1.5));
  set_boserif(0,0,10);
  push_stroke((140,320)--(860,320),
    (1.6,1.6)--(1.6,1.6));
  expand_pbox;
enddef;

vardef kanji.grthree.motion =
  push_pbox_toexpand("kanji.grthree.motion");
  build_kanji.lr(520,30)
    (kanji.leftrad.heap_big)
    (kanji.grone.power);
  expand_pbox;
enddef;

vardef kanji.grthree.next =
  push_pbox_toexpand("kanji.grthree.next");
  build_kanji.lr(360,0)
    (kanji.leftrad.ice)
    (kanji.grfour.fail);
  expand_pbox;
enddef;

vardef kanji.grthree.nose =
  push_pbox_toexpand("kanji.grthree.nose");
  build_kanji.tcb(440,-40)(140,30)
    (kanji.grtwo.self)
    (kanji.grone.paddy)
    (kanji.radical.aitch);
  expand_pbox;
enddef;

vardef kanji.grthree.once_upon_a_time =
  push_pbox_toexpand("kanji.grthree.once_upon_a_time");
  build_kanji.level(build_kanji.tb(420,-100)
    (kanji.toprad.once_upon_a_time)
    (kanji.grone.day));
  expand_pbox;
enddef;

vardef kanji.grthree.other =
  push_pbox_toexpand("kanji.grthree.other");
  build_kanji.lr(300,20)
    (kanji.leftrad.person)
    (kanji.grnine.to_be);
  expand_pbox;
enddef;

vardef kanji.grthree.pelt =
  push_pbox_toexpand("kanji.grthree.pelt");
  build_kanji.level(
    tsu_xform(identity yscaled 0.8)(
      build_kanji.cliff_enclose(
        tsu_xform(identity xyscaled (1.1,0.8) shifted (-120,-30))
          (kanji.greight.otoh));
      replace_strokep(-2)(oldp{curl 1}..(830,680)..(780,610));
      replace_strokeq(-2)(oldq..(1.5,1.5)..(1.3,1.3));
      set_boserif(-2,4,4);
      set_botip(-2,4,0));
    push_stroke((500,810)--(500,200),(1.6,1.6)--(1.5,1.5));
    set_boserif(0,0,10);
    replace_strokep(0)((point 0 of oldp)--
      (oldp intersectionpoint get_strokep(-2))));
  expand_pbox;
enddef;

vardef kanji.grthree.person =
  push_pbox_toexpand("kanji.grthree.person");
  kanji.radical.old_man(
    build_kanji.sscale(xyscaled (0.8,1.05) shifted (30*mincho,0))
      (kanji.grone.day));
  expand_pbox;
enddef;

vardef kanji.grthree.reason =
  push_pbox_toexpand("kanji.grthree.reason");
  build_kanji.box((240,600),(760,30));
  push_stroke((500,810)--(500,40),
    (1.7,1.7)--(1.5,1.5));
  set_boserif(0,0,10);
  push_stroke((240,320)--(760,320),
    (1.6,1.6)--(1.6,1.6));
  expand_pbox;
enddef;

vardef kanji.grthree.regular =
  push_pbox_toexpand("kanji.grthree.regular");
  build_kanji.tb(580,80)
    (kanji.radical.silly_hat)
    (kanji.grnine.animal_counter;
     replace_strokep(-4)(subpath (0.12,0.88) of oldp);
     replace_strokeq(-4)((1.6,1.6)--(1.6,1.6));
     set_boserif(-4,1,9));
  expand_pbox;
enddef;

vardef kanji.grthree.sheep =
  push_pbox_toexpand("kanji.grthree.sheep");
  build_kanji.sscale(yscaled 0.53 shifted (0,30))
    (kanji.grone.three);
  build_kanji.sscale(xyscaled (0.7,0.35) shifted (0,350))
    (kanji.toprad.sparkle_two);
  push_stroke((500,615)--(500,-50),(1.6,1.6)--(1.5,1.5));
  expand_pbox;
enddef;

vardef kanji.grthree.shining =
  push_pbox_toexpand("kanji.grthree.shining");
  build_kanji.lr(330,0)
    (kanji.leftrad.day)
    (kanji.greight.call);
  expand_pbox;
enddef;

vardef kanji.grthree.surface =
  push_pbox_toexpand("kanji.grthree.surface");
  build_kanji.level(build_kanji.tb(400,60)
    (kanji.toprad.blue)
    (kanji.bottomrad.clothes));
  expand_pbox;
enddef;

vardef kanji.grthree.take =
  push_pbox_toexpand("kanji.grthree.take");
  build_kanji.lr(520,50)
    (kanji.grone.ear)
    (tsu_xform(identity yscaled 0.83)(kanji.greight.otoh));
  expand_pbox;
enddef;

vardef kanji.grthree.thing =
  push_pbox_toexpand("kanji.grthree.thing");
  push_stroke((500,840),(1.6,1.6));
  set_boserif(0,0,10);
  build_kanji.add_jtail(0);
  push_stroke((80,740)--(920,740),(1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  build_kanji.box((230,620),(770,500));
  tsu_xform(identity yscaled 0.37 shifted (0,100))
    (kanji.radical.pigs_snout);
  for i=-7 upto 0: set_bosize(i,85); endfor;
  expand_pbox;
enddef;

vardef kanji.grthree.thumbtack =
  push_pbox_toexpand("kanji.grthree.thumbtack");
  push_stroke((90,760)--(910,760),(1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  push_stroke((510,760),(1.6,1.6));
  build_kanji.add_jtail(0);
  expand_pbox;
enddef;

vardef kanji.grthree.tooth =
  push_pbox_toexpand("kanji.grthree.tooth");
  build_kanji.level(build_kanji.tb(540,95)
    (kanji.grtwo.cease)
    (build_kanji.sscale(xscaled 0.86)(
       kanji.radical.open_box;
       set_boserif(-2,0,whatever);
       set_boserif(0,0,whatever);
       build_kanji.sscale(scaled 0.7 shifted (0,-20))(kanji.grtwo.rice))));
  expand_pbox;
enddef;

vardef kanji.grthree.truth =
  push_pbox_toexpand("kanji.grthree.truth");
  build_kanji.level(build_kanji.tb(200,-60)
    (build_kanji.tb(500,70)
       (kanji.grone.ten)
       (kanji.grone.eye))
    (push_stroke((100,850)--(900,850),(1.6,1.6)--(1.6,1.6));
     set_boserif(0,1,9);
     push_stroke((320,630)..tension 1.3..(220,210)..(100,-60),
       (1.6,1.6)--(1.5,1.5)--(1,1));
     set_boserif(0,0,10);
     push_stroke((630,680)..tension 1.3..(760,370)..(860,0),
       (1,1)--(1.5,1.5)--(1.8,1.8))));
  expand_pbox;
enddef;

vardef hook.world_corner(expr side_i,bottom_i) =
  if (obstacktype[side_i]=otstroke) and (obstacktype[bottom_i]=otstroke):
    if (1=length obstackp[side_i]) and (1=length obstackp[bottom_i]):
      begingroup;
        save x,y,p,e;
        numeric x[],y[],e[];
        path p[];
        
        p1=obstackp[side_i];
        p2=obstackp[bottom_i];

        z2=(direction 0.5 of p2)/abs(direction 0.5 of p2);
        z3=(direction 1 of p1)/abs(direction 1 of p1);

        if abs(z3 dotprod z2)<0.1:
          point 1 of p1=z5+e2*z3;
          z5=(point 1 of p2)+whatever*z2;
          e3=obstackna.bosize[bottom_i]*tsu_brush_max.brletter
            *tsu_brush_shape.brletter*0.5;

          if e2<e3:
            p1:=(point 0 of p1)--(z5+e3*z3);
            obstackp[side_i]:=p1;
          fi;
        fi;
      endgroup;
    fi;
  fi;
enddef;

vardef kanji.grthree.world =
  push_pbox_toexpand("kanji.grthree.world");
  build_kanji.box((200,780),(840,20));
  replace_strokep(-1)(reverse subpath (0,1) of oldp);
  replace_strokeq(-1)(reverse subpath (0,1) of oldq);
  set_boserif(-1,0,10);
  set_boserif(-1,1,whatever);
  set_botip(-1,1,whatever);
  pop_hook;
  push_hook(hsmain_render,
    "hook.world_corner("&(decimal find_stroke(-1))&","
                        &(decimal find_stroke(0))&");" );
  push_stroke((40,520)--(900,520),
    (1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  build_kanji.box((460,810),(720,280));
  set_boserif(-1,0,10);
  set_boserif(-1,1,whatever);
  set_botip(-1,1,whatever);
  push_stroke(subpath (2,3) of get_strokep(-1),
    subpath (2,3) of get_strokeq(-1));
  replace_strokep(-2)(reverse subpath (0,1) of oldp);
  replace_strokeq(-2)(reverse subpath (0,1) of oldq);
  set_boserif(0,0,10);
  expand_pbox;
enddef;
