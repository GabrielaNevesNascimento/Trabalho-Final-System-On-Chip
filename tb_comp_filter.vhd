library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity tb_comp_filter is
end entity tb_comp_filter;

architecture sim of tb_comp_filter is

    constant CLK_PERIOD : time := 10 ns; -- 100 MHz (clock padrao do Zynq PL)

    signal clk        : std_logic := '0';
    signal rst        : std_logic := '1';
    signal start      : std_logic := '0';
    signal angulo_ant : std_logic_vector(31 downto 0) := (others => '0');
    signal gyro       : std_logic_vector(31 downto 0) := (others => '0');
    signal angulo_acc : std_logic_vector(31 downto 0) := (others => '0');
    signal dt         : std_logic_vector(31 downto 0) := (others => '0');
    signal k          : std_logic_vector(31 downto 0) := (others => '0');
    signal angulo_out : std_logic_vector(31 downto 0);
    signal done       : std_logic;

    -- Constantes em Q16.16 (mesmas de filtro_ponto_fixo.c / comparacao_filtros.c)
    constant C_DT : integer := 655;  -- round(0.01 * 65536)
    constant C_K  : integer := 1311; -- round(0.02 * 65536)

begin

    -- Instancia do DUT (Device Under Test)
    uut : entity work.comp_filter
        port map (
            clk        => clk,
            rst        => rst,
            start      => start,
            angulo_ant => angulo_ant,
            gyro       => gyro,
            angulo_acc => angulo_acc,
            dt         => dt,
            k          => k,
            angulo_out => angulo_out,
            done       => done
        );

    -- Geracao de clock (100 MHz)
    clk <= not clk after CLK_PERIOD / 2;

    -- Processo de estimulos e verificacao
    stim_proc : process
        procedure run_case (
            constant nome       : in string;
            constant v_ang_ant  : in integer;
            constant v_gyro     : in integer;
            constant v_acc      : in integer;
            constant v_dt       : in integer;
            constant v_k        : in integer;
            constant esperado   : in integer
        ) is
            variable obtido : integer;
        begin
            angulo_ant <= std_logic_vector(to_signed(v_ang_ant, 32));
            gyro       <= std_logic_vector(to_signed(v_gyro, 32));
            angulo_acc <= std_logic_vector(to_signed(v_acc, 32));
            dt         <= std_logic_vector(to_signed(v_dt, 32));
            k          <= std_logic_vector(to_signed(v_k, 32));

            -- Simula escrita do ARM no registrador start = '1'
            wait until rising_edge(clk);
            start <= '1';

            -- Aguarda sinalizacao de conclusao (done = '1')
            wait until done = '1';
            wait for 1 ns;

            obtido := to_integer(signed(angulo_out));
            report nome & " -> obtido=" & integer'image(obtido) &
                   " | esperado=" & integer'image(esperado);

            assert obtido = esperado
                report "FALHA em " & nome & ": obtido=" & integer'image(obtido) &
                       ", esperado=" & integer'image(esperado)
                severity error;

            -- Simula ARM limpando start = '0' apos ler o resultado
            wait until rising_edge(clk);
            start <= '0';
            wait until rising_edge(clk);
        end procedure run_case;
    begin
        -- Reset inicial
        rst   <= '1';
        start <= '0';
        wait for CLK_PERIOD * 3;
        wait until rising_edge(clk);
        rst <= '0';
        wait until rising_edge(clk);

        -- Caso 1: Valores identicos ao main() de filtro_ponto_fixo.c
        -- angulo_ant=0.0, gyro=10.0, acc=1.0, dt=0.01, k=0.02
        run_case("Caso 1 (filtro_ponto_fixo.c)",
                 0, 655360, 65536, C_DT, C_K, 7729);

        -- Caso 2: Encadeamento usando a saida do Caso 1 como angulo_ant
        run_case("Caso 2 (iteracao seguinte)",
                 7729, 655360, 65536, C_DT, C_K, 15304);

        -- Caso 3: Valores negativos
        -- angulo_ant=-15.0 (-983040), gyro=-20.0 (-1310720), acc=-10.0 (-655360)
        run_case("Caso 3 (valores negativos)",
                 -983040, -1310720, -655360, C_DT, C_K, -989323);

        report "Todos os casos de teste passaram com sucesso!";
        wait;
    end process stim_proc;

end architecture sim;
