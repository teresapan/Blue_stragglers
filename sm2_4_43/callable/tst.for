	integer i, NXY, key
        integer fsm_device
	parameter(NXY=20)
	real x(NXY),y(NXY),err(NXY)
        real pt(NXY)
	real px, py
c
	if(fsm_device('X11') .lt. 0) then
           print *,'can''t open device'
           stop
        endif
	do 2 i=1,NXY
		x(i)=i
		y(i)=i*i
		err(i)=10
                pt(i) = 43
2	continue
c
	call fsm_graphics
        call fsm_defvar('TeX_strings','1')
	call fsm_window(2,2,2,1,2,1)
	call fsm_limits(0.0,3.0,0.0,500.0)
	call fsm_ticksize(-1.0,0.0,0.0,0.0)
	call fsm_box(1,2,3,3)
        call fsm_gflush
	call fsm_alpha
        print *,'hit any key to continue'
	call fsm_graphics
        call fsm_curs(px,py,key)
	call fsm_alpha
	print *,'You hit ',key,'at (',px,',',py,')'
        call fsm_graphics
	call fsm_window(1,1,1,1,1,1)
	call fsm_location(5000,30000,5000,30000)
	call fsm_erase
	call fsm_limits(-1.0,22.0,0.0,500.0)
	call fsm_ticksize(0.,0.,0.,0.)
	call fsm_box(1,2,0,0)
	call fsm_angle(45.0)
	call fsm_ptype(pt,NXY)
	call fsm_points(x,y,NXY)
	call fsm_angle(0.0)
	call fsm_lweight(1.0)
	call fsm_expand(2.0)
	call fsm_identification('Robert {\\oe the Good}')
	call fsm_expand(1.0)
	call fsm_xlabel('x axis')
	call fsm_ylabel('Y')
	call fsm_errorbar(x,y,err,2,NXY)
	call fsm_errorbar(x,y,err,4,NXY)
	call fsm_lweight(3.0)
	call fsm_histogram(x,y,NXY)
	call fsm_lweight(1.0)
	call fsm_ltype(2)
	call fsm_conn(x,y,NXY)
	call fsm_ltype(0)
	call fsm_relocate(10.0,100.0)
	call fsm_label('Hello ')
	call fsm_dot
	call fsm_draw(20.0,100.0)
	call fsm_putlabel(4,'Goodbye \\point60')
	call fsm_grid(0,0)
        call fsm_ltype(1)
	call fsm_grid(1,1)
        call fsm_ltype(0)
        call fsm_gflush
	call fsm_alpha
        print *,'enter a digit'
        call fsm_redraw(0)
        read(5,*) i
        print *,'enter a digit to exit'
        call fsm_redraw(0)
        read(5,*) i
        call fsm_hardcopy
	end
