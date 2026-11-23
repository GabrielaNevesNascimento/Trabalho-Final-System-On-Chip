library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Filtro complementar em ponto fixo Q16.16 (32 bits com sinal)
-- Equacao implementada:
--   pred       = angulo_anterior + (gyro * dt)
--   erro       = angulo_acc - pred
--   angulo_out = pred + (k * erro)
entity comp_filter is
    port (
        clk        : in  std_logic;
        rst        : in  std_logic; -- Reset sincrono ativo em nivel alto
        start      : in  std_logic; -- Inicia o calculo quando em '1'
        angulo_ant : in  std_logic_vector(31 downto 0); -- Q16.16
        gyro       : in  std_logic_vector(31 downto 0); -- Q16.16
        angulo_acc : in  std_logic_vector(31 downto 0); -- Q16.16
        dt         : in  std_logic_vector(31 downto 0); -- Q16.16
        k          : in  std_logic_vector(31 downto 0); -- Q16.16
        angulo_out : out std_logic_vector(31 downto 0); -- Q16.16
        done       : out std_logic  -- '1' quando o resultado esta pronto
    );
end entity comp_filter;

architecture rtl of comp_filter is
    
    -- Converte Q32.32 para Q16.16 com truncamento em direcao a zero
    function trunc_q16(prod : signed(63 downto 0))
        return signed is
        variable ajustado : signed(63 downto 0);
    begin
        ajustado := prod;
    
        -- Para numeros negativos, compensa o arredondamento
        -- para baixo realizado pelo deslocamento aritmetico
        if prod(63) = '1' then
            ajustado := prod + to_signed(65535, 64);
        end if;
    
        return ajustado(47 downto 16);
    end function;

    type state_t is (
        S_IDLE,
        S_MUL_GYRO,
        S_PRED,
        S_ERR,
        S_MUL_K,
        S_UPDATE,
        S_WAIT_START_LOW
    );

    signal state : state_t := S_IDLE;

    -- Registradores de entrada capturados no inicio do calculo
    signal r_ang_ant : signed(31 downto 0) := (others => '0');
    signal r_gyro    : signed(31 downto 0) := (others => '0');
    signal r_ang_acc : signed(31 downto 0) := (others => '0');
    signal r_dt      : signed(31 downto 0) := (others => '0');
    signal r_k       : signed(31 downto 0) := (others => '0');

    -- Registradores intermediarios do datapath
    signal r_prod1   : signed(63 downto 0) := (others => '0');
    signal r_pred    : signed(31 downto 0) := (others => '0');
    signal r_erro    : signed(31 downto 0) := (others => '0');
    signal r_prod2   : signed(63 downto 0) := (others => '0');

    signal r_out     : std_logic_vector(31 downto 0) := (others => '0');
    signal r_done    : std_logic := '0';

begin

    angulo_out <= r_out;
    done       <= r_done;

    process(clk)
    begin
        if rising_edge(clk) then
            if rst = '1' then
                state     <= S_IDLE;
                r_ang_ant <= (others => '0');
                r_gyro    <= (others => '0');
                r_ang_acc <= (others => '0');
                r_dt      <= (others => '0');
                r_k       <= (others => '0');
                r_prod1   <= (others => '0');
                r_pred    <= (others => '0');
                r_erro    <= (others => '0');
                r_prod2   <= (others => '0');
                r_out     <= (others => '0');
                r_done    <= '0';
            else
                case state is
                    when S_IDLE =>
                        if start = '1' then
                            r_ang_ant <= signed(angulo_ant);
                            r_gyro    <= signed(gyro);
                            r_ang_acc <= signed(angulo_acc);
                            r_dt      <= signed(dt);
                            r_k       <= signed(k);
                            r_done    <= '0';
                            state     <= S_MUL_GYRO;
                        end if;

                    -- 1) gyro * dt (Q16.16 * Q16.16 = Q32.32 em 64 bits)
                    when S_MUL_GYRO =>
                        r_prod1 <= r_gyro * r_dt;
                        state   <= S_PRED;

                    -- 2) pred = angulo_ant + (gyro * dt) >> 16
                    --    Bits (47 downto 16) correspondem ao ajuste de Q32.32 para Q16.16
                    when S_PRED =>
                        r_pred <= r_ang_ant + trunc_q16(r_prod1);
                        state  <= S_ERR;

                    -- 3) erro = angulo_acc - pred
                    when S_ERR =>
                        r_erro <= r_ang_acc - r_pred;
                        state  <= S_MUL_K;

                    -- 4) k * erro (Q16.16 * Q16.16 = Q32.32 em 64 bits)
                    when S_MUL_K =>
                        r_prod2 <= r_k * r_erro;
                        state   <= S_UPDATE;

                    -- 5) angulo_out = pred + (k * erro) >> 16
                    when S_UPDATE =>
                        r_out <= std_logic_vector(r_pred + trunc_q16(r_prod2));
                        r_done <= '1';
                        state  <= S_WAIT_START_LOW;

                    -- Aguarda start voltar para '0' para evitar reexecucao continua
                    -- quando controlado por registrador AXI4-Lite
                    when S_WAIT_START_LOW =>
                        if start = '0' then
                            state <= S_IDLE;
                        end if;

                    when others =>
                        state <= S_IDLE;
                end case;
            end if;
        end if;
    end process;

end architecture rtl;
