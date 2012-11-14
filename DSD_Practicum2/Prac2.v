//Joshua Beninson


////////////////////////////////////////////////////////////////////////
module FSM(q,init,clk,pstate,nstate,out,z);

input q,init,clk;

output reg z;
output reg[0:4] pstate,nstate;
output reg[0:4] out;


parameter START = 0, S_1 = 1, S_2 = 2, S_3 = 3, S_4 = 4, S_5 = 5;


	always@(negedge clk or negedge init)
	begin
		if(init==0)
			begin
			pstate = S_1;
			end
		else 
			begin
			pstate = nstate;
			end			
	end

	always@(pstate or q or negedge clk)
	begin
		case(pstate)
		S_1:
			begin
				if(q==0)
					begin
					z = 0;
					nstate = 3;
					end
				else if(q==1)
					begin
					z=0;
					nstate = 2;
					end
			end
		S_2:
			begin
				if(q==0)
					begin
					z=0;
					nstate = 1;
					end
				else if(q==1)
					begin
					z=0;
					nstate = 1;
					end
			end
		S_3:
			begin
				if(q==0)
					begin
					z=0;
					nstate = 4;
					end
				else if(q==1)
					begin
					z=0;
					nstate = 2;
					end
			end
		S_4:
			begin
				if(q==0)
					begin
					z=0;
					nstate = 2;
					end
				else if(q==1)
					begin
					z=0;
					nstate = 5;
					end
			end
		S_5:
			begin

				if(q==0)
					begin
						if(out == 18)
							begin
							z=0;
							out = 0; 
							nstate = 3;
							end
						else
							begin
							z = 1;
							out = out + 1; 
							nstate = 5;
							end
													
					end
				else if(q==1)
					begin
						out = 0;
						nstate = 5;
					end

	
			end
		endcase
	end


endmodule

///////////////////////////////////////////////////////////////////////////////


module connector();


	wire q,init,clk,z;
	wire [0:4] pstate,nstate;
	wire [0:4] out;


	FSM thisfsm(q,init,clk,pstate,nstate,out,z);
	testbench tb(q,init,clk,pstate,nstate,out,z);
endmodule

/////////////////////////////////////////////////////////////////////////////

module testbench (q,init,clk,pstate,nstate,out,z);

output reg init,clk,q;
input z;
input [0:4] pstate,nstate,out;

initial
	begin
	$dumpvars;
	$dumpfile("fsm.dump");
	    init = 0; 
	    clk = 1;
	    q = 0;
	#5  clk = 0; 
	    init = 1;
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 1;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 1;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 1;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 1;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 1;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;  
	    clk = 1;
	#5  clk = 0; 
	#5  q = 0;
	    clk = 1;
	#5  clk = 0; 
	end
endmodule
