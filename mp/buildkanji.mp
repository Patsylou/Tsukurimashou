%
% Build a kanji character by assembling parts
% Copyright (C) 2011, 2012, 2013  Matthew Skala
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

inclusion_lock(buildkanji);

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% a beret is a tilted hat
vardef build_kanji.add_beret(text curves) =
  perl_structure:=perl_structure
    &"['build_kanji.add_beret','eids.u2FF1.u4E3F._1',";
  begingroup
    save osp;
    numeric osp;
    osp:=sp;
    curves;
    save i,lox,hix,toppt,myxf;
    lox:=infinity;
    hix:=-infinity;
    pair toppt;
    toppt:=(500,-infinity);
    i:=0;
    forever:
      exitif find_stroke(i)<osp;
      if xpart llcorner get_strokep(i)<lox:
        lox:=xpart llcorner get_strokep(i);
      fi;
      if xpart urcorner get_strokep(i)>hix:
        hix:=xpart urcorner get_strokep(i);
      fi;
      if ypart point 0 of get_strokep(i)>ypart toppt:
        toppt:=point 0 of get_strokep(i);
      fi;
      if ypart point infinity of get_strokep(i)>ypart toppt:
        toppt:=point infinity of get_strokep(i);
      fi;
      i:=i-1;
    endfor;
    i:=0;
    forever:
      exitif find_stroke(i)<osp;
      if point 0 of get_strokep(i)=toppt:
        set_boserif(i,0,whatever);
      fi;
      if point infinity of get_strokep(i)=toppt:
        set_boserif(i,length get_strokep(i),whatever);
      fi;
      i:=i-1;
    endfor;
    transform myxf;
    (-500,810) transformed myxf=(lox,(ypart toppt)+30);
    (500,900) transformed myxf=(hix,900);
    (0,780) transformed myxf=toppt;
    push_stroke(
      ((-400,750)..(0,780)..tension 1.1..(360,840)) transformed myxf,
      (1.1,1.1)--(1.6,1.6)--(2.0,2.0));
  endgroup;
  perl_structure:=perl_structure&"],";
enddef;

vardef build_kanji.add_jtail(expr idx) =
  replace_strokep(idx)(oldp--(xpart point infinity of oldp,30){down}..
    {curl 0.2}((xpart point infinity of oldp)-150,0));
  replace_strokep(idx)(insert_nodes(oldp)((length oldp)-0.5));
  replace_strokeq(idx)(oldq--(1.5,1.5)--(1.4,1.4)--(1.2,1.2));
enddef;

% hook used by extend_ltail_enclose
numeric last_ltail;

vardef build_kanji.add_ltail(expr idx) =
  begingroup
    save x,y;
    numeric x[],y[];
    z1=point infinity of get_strokep(idx);
    last_ltail:=find_stroke(idx);
    x2=x1;
    y2=80-40*mincho;
    x3=0.4[x2,800];
    y3=mincho[0,-20];
    replace_strokep(idx)(interpath(mincho,
      oldp--z2{down}...{right}z3..(750,0)..(810,30)..
        tension 2..(850,230),
      oldp--z2{down}...{right}z3..(850,0){curl 0.2}..(810,60)..
        tension 2..(810,170)));
    replace_strokeq(idx)(oldq--(1.6,1.6)--(1.75,1.75)--(1.9,1.9)
      --(1.3,1.3)--(0.8,0.8));
  endgroup;
enddef;

vardef build_kanji.attach_fishhook(expr scaleamt)(text curves) =
  perl_structure:=
    perl_structure&"['build_kanji.attach_fishhook','eids.u2FF1.u2E88._2',[";
  begingroup
    save osp;
    numeric osp;
    osp:=sp;
    tsu_xform(identity shifted (0,50) yscaled scaleamt shifted (0,-50))
      (curves);
    perl_structure:=perl_structure&"],";
    save i,j,x,y,pp,myxf;
    path pp;
    transform myxf;
    z1=z2=(-infinity,-infinity);
    i:=0;
    forever:
      exitif find_stroke(i)<osp;
      pp:=get_strokep(i) rotated -45;
      for j=0 upto length pp:
        x3:=xpart point j of pp;
        y3:=ypart point j of pp;
        if x3>x1:
          x1:=x3;
          y1:=y3;
        fi;
        if y3>y2:
          x2:=x3;
          y2:=y3;
        fi;
      endfor;
      i:=i-1;
    endfor;
    (0,0) transformed myxf=(z2 rotated 45);
    (1.8,0) transformed myxf=(z1 rotated 45);
    xypart myxf=0;
    ypart ((0,1) transformed myxf)=830;
    push_stroke(
      ((0.5,0.9)..tension 1.2..(0,0)..(-0.5,-0.5)) transformed myxf,
      (1.7,1.7)--(1.3,1.3)--(1,1));
    set_boserif(0,0,10);
    push_stroke(
      ((0.4,0.65)--(1.3,0.65)..tension 1.2..(1.14,0.3)..(0.88,0))
         transformed myxf,
      (1.6,1.6)--(1.6,1.6)--(1.4,1.4)--(1,1));
    set_boserif(0,1,4);
    set_botip(0,1,0);
  endgroup;
  perl_structure:=perl_structure&"],";
enddef;

vardef build_kanji.attach_tick(expr newtop)(text curves) =
  perl_structure:=
    perl_structure&"['build_kanji.attach_fishhook','eids.u2FF1.u31D2._1',[";
  begingroup
    save atosp,atnsp;
    numeric atosp,atnsp;
    atosp:=sp;
    curves;
    perl_structure:=perl_structure&"],";
    atnsp:=sp;
    save i,thisy,maxy;
    maxy:=-infinity;
    i:=0;
    forever:
      exitif find_stroke(i)<atosp;
      if (xpart (get_strokep(i) intersectiontimes
            ((400,900)--(500,200)--(600,900))))>=0:
        thisy:=ypart urcorner get_strokep(i);
        if thisy>maxy:
          maxy:=thisy;
        fi;
      fi;
      i:=i-1;
    endfor;
    sp:=atosp;
    tsu_xform(identity yscaled (newtop/maxy))(sp:=atnsp);
    push_stroke((500,190+newtop)--(440,newtop),(1.7,1.7)--(1,1));
    set_boserif(0,0,10);
  endgroup;
  perl_structure:=perl_structure&"],";
enddef;

vardef hook.box_bottom(expr sides_i,bottom_i) =
  if (obstacktype[sides_i]=otstroke) and (obstacktype[bottom_i]=otstroke):
    if (3=length obstackp[sides_i]) and (1=length obstackp[bottom_i]):
      begingroup;
        save x,y,p,e;
        numeric x[],y[],e[];
        path p[];
        
        p1=obstackp[sides_i];
        p2=obstackp[bottom_i];

        z1=-(direction 0 of p1)/abs(direction 0 of p1);
        z2=(direction 0.5 of p2)/abs(direction 0.5 of p2);
        z3=(direction 3 of p1)/abs(direction 3 of p1);

        if (abs(z1 dotprod z2)<0.1) and (abs(z3 dotprod z2)<0.1):
          point 0 of p1=z4+e1*z1;
          z4=(point 0 of p2)+whatever*z2;
          point 3 of p1=z5+e2*z3;
          z5=(point 1 of p2)+whatever*z2;
          e3=obstackna.bosize[bottom_i]*tsu_brush_max.brletter
            *tsu_brush_shape.brletter*0.5;

          if e1<e3:
            p1:=(z4+e3*z1)--(subpath (1,3) of p1);
          fi;
          if e2<e3:
            p1:=(subpath (0,2) of p1)--(z5+e3*z3);
          fi;
          obstackp[sides_i]:=p1;
        fi;
      endgroup;
    fi;
  fi;
enddef;

vardef build_kanji.box(expr ul,lr) =
  perl_structure:=perl_structure&"['build_kanji.box',";
  begingroup
    save boxext;
    if (ypart (ul-lr))>500:
      boxext:=-100/(ypart (ul-lr));
    else:
      boxext:=-0.2;
    fi;
    if (boxext)[ypart lr,ypart ul]<-60:
      boxext:=(-60+ypart lr)/(ypart ul-ypart lr);
    fi;
    push_stroke((xpart ul,(boxext)[ypart lr,ypart ul])--ul--
        (xpart lr,ypart ul)--(xpart lr,(boxext)[ypart lr,ypart ul]),
      (1.5,1.5)--(1.7,1.7)--(1.7,1.7)--(1.5,1.5));
  endgroup;
  set_botip(0,1,1);
  set_botip(0,2,1);
  set_boserif(0,1,4);
  set_boserif(0,2,4);
  push_stroke((xpart ul,ypart lr)--lr,
    (1.5,1.5)--(1.5,1.5));
  push_hook(hsmain_render,
    "hook.box_bottom("&(decimal find_stroke(-1))&","
                      &(decimal find_stroke(0))&");" );
  perl_structure:=perl_structure&"],";
enddef;

vardef build_kanji.cup(expr ul,lr) =
  push_stroke(ul--(xpart ul,(-0.2)[ypart lr,ypart ul]),
    (1.6,1.6)--(1.4,1.4));
  set_boserif(0,0,10);
  push_stroke((xpart lr,ypart ul)--(xpart lr,(-0.2)[ypart lr,ypart ul]),
    (1.6,1.6)--(1.4,1.4));
  set_boserif(0,0,10);
  push_stroke((xpart ul,ypart lr)--lr,
    (1.5,1.5)--(1.5,1.5));
  perl_structure:=perl_structure&"'build_kanji.cup',";
enddef;

vardef build_kanji.harmonic(expr gap,sval,lspread)(text curves) =
  perl_structure:=perl_structure
    &"['build_kanji.harmonic','eids.u2FF1.u003F._1',[";
  begingroup
    save myxf;
    transform myxf;
    (50,-50) transformed myxf=(50,-50);
    (950,-50) transformed myxf=(950,-50);
    (50,850) transformed myxf=(50,850-gap);
    tsu_xform(myxf)(curves);
    perl_structure:=perl_structure&"],[";
    save hsp;
    hsp:=sp;
    tsu_xform(myxf)(build_kanji.spread_legs(lspread)(curves));
    save i,tp,toclip,nadded;
    numeric i,toclip,nadded;
    path tp;
    i:=0;
    toclip:=0;
    nadded:=0;
    forever:
      tp:=get_strokep(i);
      if (ypart ulcorner tp<450)
        or (abs(ypart (direction 0 of tp)/abs(direction 0 of tp))>0.95)
        or (abs(ypart (direction infinity of tp)/
          abs(direction infinity of tp))>0.95):
        set_bosize(i,0);
        toclip:=toclip+1;
      else:
        replace_strokep(i)
          (tp shifted -(0.5[ulcorner tp,lrcorner tp])
              scaled sval
              shifted ((0,gap)+0.5[ulcorner tp,lrcorner tp]));
        set_bosize(i)(get_bosize(i)*sqrt(sval));
        toclip:=toclip+2;
        nadded:=nadded+1;
      fi;
      i:=i-1;
      exitif find_stroke(i)<hsp;
    endfor;
  endgroup;
  perl_structure:=perl_structure&"]],";
enddef;

vardef build_kanji.lcr(expr splitpointa,overlapa)(expr splitpointb,overlapb)
  (text leftstuff)(text centrestuff)(text rightstuff) =
  perl_structure:=perl_structure
    &"['build_kanji.lcr','eids.u2ff2._2.1_1.2_',[";
  begingroup
    save t;
    transform t[];
    yypart t1=yypart t2=yypart t3=1;
    ypart t1=yxpart t1=xypart t1=0;
    ypart t2=yxpart t2=xypart t2=0;
    ypart t3=yxpart t3=xypart t3=0;
    (50,0) transformed t1=(50,0);
    (950,0) transformed t1=(splitpointa+overlapa/2,0);
    (50,0) transformed t2=(splitpointa-overlapa/2,0);
    (950,0) transformed t2=(splitpointb+overlapb/2,0);
    (50,0) transformed t3=(splitpointb-overlapb/2,0);
    (950,0) transformed t3=(950,0);
    tsu_xform(t1)(leftstuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t2)(centrestuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t3)(rightstuff);
  endgroup;
  perl_structure:=perl_structure&"]],";
enddef;

vardef build_kanji.lean_to(expr lr) =
  push_stroke((120,620)--(880,620),(1.6,1.6)--(1.6,1.6));
  set_boserif(0,1,9);
  begingroup
    save ltxf;
    transform ltxf;
    xypart ltxf=yxpart ltxf=0;
    (60,800) transformed ltxf=(60,800);
    (360,-30) transformed ltxf=lr;
    push_stroke(
      ((360,800)..tension 1.2..(270,300)..(60,-30)) transformed ltxf,
      (1.6,1.6)--(1.4,1.4)--(0.9,0.9));
  endgroup;
  set_boserif(0,0,10);
enddef;

vardef build_kanji.level(text curves) =
  begingroup
    save xsp;
    xsp:=sp;
    curves;
    save lsum,denom,i;
    lsum:=0;
    denom:=0;
    i:=0;
    forever:
      exitif find_stroke(i)<xsp;
      if unknown get_bosize(i):
        set_bosize(i,100);
      fi;
      if (get_bosize(i)>0):
        lsum:=lsum+mlog(get_bosize(i));
        denom:=denom+1;
      fi;
      i:=i-1;
    endfor;
    i:=0;
    forever:
      exitif find_stroke(i)<xsp;
      if get_bosize(i)>0:
        set_bosize(i,mexp(lsum/denom));
      fi;
      i:=i-1;
    endfor;
  endgroup;
enddef;

vardef build_kanji.lstransform(expr thresh,dist,mypt) =
  if ypart mypt>thresh:
    mypt
  else:
    (xpart mypt,
      (ypart mypt)*((thresh-dist)/thresh)
     +(xpart mypt/1000)[(dist/thresh)*ypart mypt,dist])
  fi
enddef;

vardef build_kanji.lift_skirt(expr thresh,dist)(text curves) =
  begingroup
    save osp;
    numeric osp;
    osp:=sp;
    curves;
    save i,boti,x,y;
    numeric x[],y[];
    i:=0;
    boti:=whatever;
    y0:=1000;
    forever:
      exitif find_stroke(i)<osp;
      if (ypart llcorner get_strokep(i))=(ypart urcorner get_strokep(i)):
        if (unknown boti) or (ypart llcorner get_strokep(i)<y0):
          y0:=ypart llcorner get_strokep(i);
          boti:=i;
        fi;
      fi;
      replace_strokep(i)(
        for j=0 upto length oldp-1:
          build_kanji.lstransform(thresh,dist)(point j of oldp)
          ..controls build_kanji.lstransform(thresh,dist)(postcontrol j of oldp)
          and build_kanji.lstransform(thresh,dist)(precontrol j+1 of oldp)..
        endfor
        if cycle oldp:
          cycle
        else:
          build_kanji.lstransform(thresh,dist)(point infinity of oldp)
        fi);
      i:=i-1;
    endfor;
    if (known boti) and (y0<=thresh):
      replace_strokep(boti)
        (((0,7)+point 0 of oldp)..tension 1.2..((0,-5)+point 0.5 of oldp)..
         ((0,10)+point 1 of oldp));
      replace_strokeq(boti)((1.7,1.7)--(1.5,1.5)--(1,1));
      set_boserif(boti,1,whatever);
    fi;
  endgroup;
enddef;

vardef build_kanji.lr(expr splitpoint,overlap)
  (text leftstuff)(text rightstuff) =
  perl_structure:=perl_structure
    &"['build_kanji.lr','eids.u2ff0._1.1_',[";
  begingroup
    save t;
    transform t[];
    yypart t1=yypart t2=1;
    ypart t1=yxpart t1=xypart t1=ypart t2=yxpart t2=xypart t2=0;
    (50,0) transformed t1=(50,0);
    (950,0) transformed t1=(splitpoint+overlap/2,0);
    (50,0) transformed t2=(splitpoint-overlap/2,0);
    (950,0) transformed t2=(950,0);
    tsu_xform(t1)(leftstuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t2)(rightstuff);
  endgroup;
  perl_structure:=perl_structure&"]],";
enddef;

vardef build_kanji.sscale(text tran)(text curves) =
  tsu_xform(identity shifted (-centre_pt) tran shifted centre_pt)(curves);
enddef;

vardef build_kanji.spread_legs(expr dist)(text curves) =
  begingroup
    save osp;
    numeric osp;
    osp:=sp;
    curves;
    save mytr;
    transform mytr[];
    (50,-50) transformed mytr1=(50,-50);
    (500,-50) transformed mytr1=(500-dist/2,-50);
    (500,850) transformed mytr1=(500-dist/2,850);
    (950,-50) transformed mytr2=(950,-50);
    (500,-50) transformed mytr2=(500+dist/2,-50);
    (500,850) transformed mytr2=(500+dist/2,850);
    save i;
    i:=0;
    forever:
      exitif find_stroke(i)<osp;
      if xpart 0.75[llcorner get_strokep(i),urcorner get_strokep(i)]<475:
        replace_strokep(i,oldp transformed mytr1);
      elseif xpart 0.25[llcorner get_strokep(i),urcorner get_strokep(i)]>525:
        replace_strokep(i,oldp transformed mytr2);
      fi;
      i:=i-1;
    endfor;
  endgroup;
enddef;

vardef build_kanji.tb(expr splitpoint,overlap)
  (text topstuff)(text bottomstuff) =
  perl_structure:=perl_structure
    &"['build_kanji.tb','eids.u2ff1._1.1_',[";
  begingroup
    save t;
    transform t[];
    xxpart t1=xxpart t2=1;
    xpart t1=xypart t1=yxpart t1=xpart t2=xypart t2=yxpart t2=0;
    (0,850) transformed t1=(0,850);
    (0,-50) transformed t1=(0,splitpoint-overlap/2);
    (0,850) transformed t2=(0,splitpoint+overlap/2);
    (0,-50) transformed t2=(0,-50);
    tsu_xform(t1)(topstuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t2)(bottomstuff);
  endgroup;
  perl_structure:=perl_structure&"]],";
enddef;

vardef build_kanji.tcb(expr splitpointa,overlapa)(expr splitpointb,overlapb)
  (text topstuff)(text centrestuff)(text bottomstuff) =
  perl_structure:=perl_structure
    &"['build_kanji.tcb','eids.u2ff3._2.1_1.2_',[";
  begingroup
    save t;
    transform t[];
    xxpart t1=xxpart t2=xxpart t3=1;
    xpart t1=xypart t1=yxpart t1=0;
    xpart t2=xypart t2=yxpart t2=0;
    xpart t3=xypart t3=yxpart t3=0;
    (0,850) transformed t1=(0,850);
    (0,-50) transformed t1=(0,splitpointa-overlapa/2);
    (0,850) transformed t2=(0,splitpointa+overlapa/2);
    (0,-50) transformed t2=(0,splitpointb-overlapb/2);
    (0,850) transformed t3=(0,splitpointb+overlapb/2);
    (0,-50) transformed t3=(0,-50);
    tsu_xform(t1)(topstuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t2)(centrestuff);
    perl_structure:=perl_structure&"],[";
    tsu_xform(t3)(bottomstuff);
  endgroup;
  perl_structure:=perl_structure&"]],";
enddef;

vardef build_kanji.thickness(expr amount)(text curves) =
  begingroup
    save xsp;
    xsp:=sp;
    curves;
    i:=0;
    forever:
      exitif find_stroke(i)<xsp;
      if unknown get_bosize(i):
        set_bosize(i,100);
      fi;
      if get_bosize(i)>0:
        set_bosize(i,get_bosize(i)*amount);
      fi;
      i:=i-1;
    endfor;
  endgroup;
enddef;

vardef build_kanji.tricluster(expr topxscale)
  (text topstuff)(text leftstuff)(text rightstuff) =
  build_kanji.tb(500,0)
    (build_kanji.sscale(xscaled topxscale)(topstuff))
    (build_kanji.lr(480,0)
      (leftstuff)
      (rightstuff));
enddef;

vardef build_kanji.wptransform(expr ca,cb,cc,cd)(expr inpt) =
  begingroup
    save myxf,x,y;
    transform myxf;
    numeric x[],y[];
    (50,-50) transformed myxf=(0,0);
    (50,850) transformed myxf=(0,1);
    (950,-50) transformed myxf=(1,0);
    z1=inpt transformed myxf;
    z2=y1[x1[cc,cd],x1[cb,ca]];
    z2 % no semicolon
  endgroup % no semicolon
enddef;

vardef build_kanji.warp(expr ca,cb,cc,cd)(text curves) =
  begingroup
    save osp;
    numeric osp;
    osp:=sp;
    curves;
    save i;
    i:=0;
    forever:
      exitif find_stroke(i)<osp;
      replace_strokep(i)(
        for j=0 upto length oldp-1:
          build_kanji.wptransform(ca,cb,cc,cd)(point j of oldp)
          ..controls build_kanji.wptransform(ca,cb,cc,cd)(postcontrol j of oldp)
          and build_kanji.wptransform(ca,cb,cc,cd)(precontrol j+1 of oldp)..
        endfor
        if cycle oldp:
          cycle
        else:
          build_kanji.wptransform(ca,cb,cc,cd)(point infinity of oldp)
        fi);
      i:=i-1;
    endfor;
  endgroup;
enddef;
