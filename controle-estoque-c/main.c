/*
 * Venda e Controle de Estoque - manipulacao de arquivos binarios em C
 *
 * Dois arquivos binarios, cada um com um tipo de registro:
 *   - produtos.dat : registros do tipo Produto
 *   - vendas.dat   : registros do tipo Venda (possui um vetor com os
 *                    IDs dos produtos vendidos -> relacionamento)
 *
 * Operacoes: cadastrar, ler um registro, ler todos, atualizar, remover
 * (copiando para um arquivo novo) e relatorios que mostram a associacao
 * entre vendas e produtos. Ao registrar ou cancelar uma venda, o estoque
 * dos produtos e atualizado automaticamente.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ_PRODUTOS "produtos.dat"
#define ARQ_VENDAS   "vendas.dat"
#define ARQ_TEMP     "temp.dat"
#define MAX_ITENS    10
#define ESTOQUE_BAIXO 5

/* IMPORTANTE: em ambas as structs o campo "id" e o primeiro,
   o que permite usar a mesma funcao de remocao para os dois arquivos. */
typedef struct {
    int   id;
    char  descricao[100];
    float preco;
    int   estoque;
} Produto;

typedef struct {
    int   id;
    char  cliente[80];
    char  data[11];                 /* dd/mm/aaaa */
    int   qtd_itens;
    int   itens[MAX_ITENS];         /* IDs dos produtos (produtos.dat) */
    int   quantidades[MAX_ITENS];   /* quantidade vendida de cada item */
    float precos[MAX_ITENS];        /* preco unitario no momento da venda */
    float total;
} Venda;

/* ------------------------------------------------------------------ */
/* Funcoes auxiliares de entrada                                       */
/* ------------------------------------------------------------------ */

static void ler_texto(const char *prompt, char *dest, size_t tam) {
    printf("%s", prompt);
    if (!fgets(dest, (int)tam, stdin)) {
        dest[0] = '\0';
        return;
    }
    char *nl = strchr(dest, '\n');
    if (nl) {
        *nl = '\0';
    } else {
        /* entrada maior que o buffer: descarta o resto da linha */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
}

static int ler_int(const char *prompt) {
    char buf[64];
    printf("%s", prompt);
    if (!fgets(buf, sizeof buf, stdin)) return 0;
    return atoi(buf);
}

static float ler_float(const char *prompt) {
    char buf[64];
    printf("%s", prompt);
    if (!fgets(buf, sizeof buf, stdin)) return 0.0f;
    return (float)atof(buf);
}

/* ------------------------------------------------------------------ */
/* Funcoes genericas / de busca                                        */
/* ------------------------------------------------------------------ */

/* Retorna o proximo ID livre (maior ID existente + 1). */
static int proximo_id(const char *arquivo, size_t tam) {
    FILE *f = fopen(arquivo, "rb");
    if (!f) return 1;
    void *buf = malloc(tam);
    int maior = 0;
    while (fread(buf, tam, 1, f) == 1) {
        int id;
        memcpy(&id, buf, sizeof(int));
        if (id > maior) maior = id;
    }
    free(buf);
    fclose(f);
    return maior + 1;
}

/* Procura produto pelo ID. Retorna a posicao (indice) no arquivo ou -1. */
static int buscar_produto(int id, Produto *out) {
    FILE *f = fopen(ARQ_PRODUTOS, "rb");
    if (!f) return -1;
    Produto p;
    int pos = 0;
    while (fread(&p, sizeof(Produto), 1, f) == 1) {
        if (p.id == id) {
            if (out) *out = p;
            fclose(f);
            return pos;
        }
        pos++;
    }
    fclose(f);
    return -1;
}

/* Procura venda pelo ID. Retorna a posicao (indice) ou -1. */
static int buscar_venda(int id, Venda *out) {
    FILE *f = fopen(ARQ_VENDAS, "rb");
    if (!f) return -1;
    Venda v;
    int pos = 0;
    while (fread(&v, sizeof(Venda), 1, f) == 1) {
        if (v.id == id) {
            if (out) *out = v;
            fclose(f);
            return pos;
        }
        pos++;
    }
    fclose(f);
    return -1;
}

/* Sobrescreve o produto na posicao informada (atualizacao no arquivo). */
static int gravar_produto_pos(int pos, const Produto *p) {
    FILE *f = fopen(ARQ_PRODUTOS, "r+b");
    if (!f) return 0;
    fseek(f, (long)pos * (long)sizeof(Produto), SEEK_SET);
    fwrite(p, sizeof(Produto), 1, f);
    fclose(f);
    return 1;
}

/* Sobrescreve a venda na posicao informada. */
static int gravar_venda_pos(int pos, const Venda *v) {
    FILE *f = fopen(ARQ_VENDAS, "r+b");
    if (!f) return 0;
    fseek(f, (long)pos * (long)sizeof(Venda), SEEK_SET);
    fwrite(v, sizeof(Venda), 1, f);
    fclose(f);
    return 1;
}

/*
 * Remocao: cria um arquivo novo com todos os registros, menos o que
 * sera excluido; fecha os dois, apaga o antigo e renomeia o novo.
 * Retorna 1 se removeu, 0 se o ID nao existe, -1 em caso de erro.
 */
static int remover_registro(const char *arquivo, size_t tam, int id) {
    FILE *orig = fopen(arquivo, "rb");
    if (!orig) return 0;
    FILE *novo = fopen(ARQ_TEMP, "wb");
    if (!novo) {
        fclose(orig);
        return -1;
    }

    void *buf = malloc(tam);
    int removido = 0;
    while (fread(buf, tam, 1, orig) == 1) {
        int rid;
        memcpy(&rid, buf, sizeof(int));
        if (rid == id) {
            removido = 1;       /* nao copia este registro */
        } else {
            fwrite(buf, tam, 1, novo);
        }
    }
    free(buf);
    fclose(orig);
    fclose(novo);

    if (!removido) {
        remove(ARQ_TEMP);
        return 0;
    }
    if (remove(arquivo) != 0) return -1;
    if (rename(ARQ_TEMP, arquivo) != 0) return -1;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Impressao                                                           */
/* ------------------------------------------------------------------ */

static void imprimir_produto(const Produto *p) {
    printf("ID: %d | Descricao: %s | Preco: R$ %.2f | Estoque: %d\n",
           p->id, p->descricao, p->preco, p->estoque);
}

static void imprimir_venda(const Venda *v) {
    printf("ID: %d | Cliente: %s | Data: %s | Itens: %d | Total: R$ %.2f\n",
           v->id, v->cliente, v->data, v->qtd_itens, v->total);
}

/* ------------------------------------------------------------------ */
/* Produtos                                                            */
/* ------------------------------------------------------------------ */

static void cadastrar_produto(void) {
    Produto p;
    memset(&p, 0, sizeof p);
    p.id = proximo_id(ARQ_PRODUTOS, sizeof(Produto));

    ler_texto("Descricao: ", p.descricao, sizeof p.descricao);
    p.preco   = ler_float("Preco (R$): ");
    p.estoque = ler_int("Quantidade em estoque: ");

    FILE *f = fopen(ARQ_PRODUTOS, "ab");
    if (!f) {
        printf("Erro ao abrir %s.\n", ARQ_PRODUTOS);
        return;
    }
    fwrite(&p, sizeof(Produto), 1, f);
    fclose(f);
    printf("Produto cadastrado com ID %d.\n", p.id);
}

static void listar_produtos(void) {
    FILE *f = fopen(ARQ_PRODUTOS, "rb");
    if (!f) {
        printf("Nenhum produto cadastrado.\n");
        return;
    }
    Produto p;
    int qtd = 0;
    printf("\n--- Todos os produtos ---\n");
    while (fread(&p, sizeof(Produto), 1, f) == 1) {
        imprimir_produto(&p);
        qtd++;
    }
    fclose(f);
    if (qtd == 0) printf("Nenhum produto cadastrado.\n");
}

static void consultar_produto(void) {
    int id = ler_int("ID do produto: ");
    Produto p;
    if (buscar_produto(id, &p) < 0)
        printf("Produto %d nao encontrado.\n", id);
    else
        imprimir_produto(&p);
}

static void atualizar_produto(void) {
    int id = ler_int("ID do produto a atualizar: ");
    Produto p;
    int pos = buscar_produto(id, &p);
    if (pos < 0) {
        printf("Produto %d nao encontrado.\n", id);
        return;
    }

    printf("Registro atual: ");
    imprimir_produto(&p);
    printf("(Pressione Enter para manter o valor atual)\n");

    char buf[100];
    ler_texto("Nova descricao: ", buf, sizeof buf);
    if (buf[0]) strcpy(p.descricao, buf);
    ler_texto("Novo preco: ", buf, sizeof buf);
    if (buf[0]) p.preco = (float)atof(buf);
    ler_texto("Novo estoque: ", buf, sizeof buf);
    if (buf[0]) p.estoque = atoi(buf);

    if (gravar_produto_pos(pos, &p))
        printf("Produto atualizado.\n");
    else
        printf("Erro ao abrir %s.\n", ARQ_PRODUTOS);
}

static void remover_produto(void) {
    int id = ler_int("ID do produto a remover: ");
    int r = remover_registro(ARQ_PRODUTOS, sizeof(Produto), id);
    if (r == 1)
        printf("Produto removido. (Vendas antigas continuam registradas.)\n");
    else if (r == 0)
        printf("Produto %d nao encontrado.\n", id);
    else
        printf("Erro ao remover o produto.\n");
}

/* ------------------------------------------------------------------ */
/* Vendas                                                              */
/* ------------------------------------------------------------------ */

static void registrar_venda(void) {
    Venda v;
    memset(&v, 0, sizeof v);
    v.id = proximo_id(ARQ_VENDAS, sizeof(Venda));

    ler_texto("Cliente: ", v.cliente, sizeof v.cliente);
    ler_texto("Data (dd/mm/aaaa): ", v.data, sizeof v.data);

    printf("Informe os produtos da venda (ID 0 para terminar, max %d itens):\n",
           MAX_ITENS);
    while (v.qtd_itens < MAX_ITENS) {
        int idp = ler_int("  ID do produto: ");
        if (idp == 0) break;

        Produto p;
        if (buscar_produto(idp, &p) < 0) {
            printf("  Produto %d nao existe.\n", idp);
            continue;
        }
        int repetido = 0;
        for (int i = 0; i < v.qtd_itens; i++)
            if (v.itens[i] == idp) repetido = 1;
        if (repetido) {
            printf("  Produto ja incluido nesta venda.\n");
            continue;
        }

        printf("  %s - R$ %.2f (estoque: %d)\n", p.descricao, p.preco, p.estoque);
        int q = ler_int("  Quantidade: ");
        if (q <= 0) {
            printf("  Quantidade invalida.\n");
            continue;
        }
        if (q > p.estoque) {
            printf("  Estoque insuficiente (disponivel: %d).\n", p.estoque);
            continue;
        }

        v.itens[v.qtd_itens]       = idp;
        v.quantidades[v.qtd_itens] = q;
        v.precos[v.qtd_itens]      = p.preco;
        v.total += p.preco * (float)q;
        v.qtd_itens++;
    }

    if (v.qtd_itens == 0) {
        printf("Venda sem itens: cancelada.\n");
        return;
    }

    /* Baixa no estoque (atualiza os registros no arquivo de produtos). */
    for (int i = 0; i < v.qtd_itens; i++) {
        Produto p;
        int pos = buscar_produto(v.itens[i], &p);
        if (pos >= 0) {
            p.estoque -= v.quantidades[i];
            gravar_produto_pos(pos, &p);
        }
    }

    FILE *f = fopen(ARQ_VENDAS, "ab");
    if (!f) {
        printf("Erro ao abrir %s.\n", ARQ_VENDAS);
        return;
    }
    fwrite(&v, sizeof(Venda), 1, f);
    fclose(f);
    printf("Venda registrada com ID %d. Total: R$ %.2f\n", v.id, v.total);
}

static void listar_vendas(void) {
    FILE *f = fopen(ARQ_VENDAS, "rb");
    if (!f) {
        printf("Nenhuma venda registrada.\n");
        return;
    }
    Venda v;
    int qtd = 0;
    printf("\n--- Todas as vendas ---\n");
    while (fread(&v, sizeof(Venda), 1, f) == 1) {
        imprimir_venda(&v);
        qtd++;
    }
    fclose(f);
    if (qtd == 0) printf("Nenhuma venda registrada.\n");
}

static void consultar_venda(void) {
    int id = ler_int("ID da venda: ");
    Venda v;
    if (buscar_venda(id, &v) < 0)
        printf("Venda %d nao encontrada.\n", id);
    else
        imprimir_venda(&v);
}

static void atualizar_venda(void) {
    int id = ler_int("ID da venda a atualizar: ");
    Venda v;
    int pos = buscar_venda(id, &v);
    if (pos < 0) {
        printf("Venda %d nao encontrada.\n", id);
        return;
    }

    printf("Registro atual: ");
    imprimir_venda(&v);
    printf("(Pressione Enter para manter o valor atual)\n");

    char buf[100];
    ler_texto("Novo cliente: ", buf, sizeof buf);
    if (buf[0]) strcpy(v.cliente, buf);
    ler_texto("Nova data (dd/mm/aaaa): ", buf, sizeof buf);
    if (buf[0]) {
        strncpy(v.data, buf, sizeof v.data - 1);
        v.data[sizeof v.data - 1] = '\0';
    }

    if (gravar_venda_pos(pos, &v))
        printf("Venda atualizada.\n");
    else
        printf("Erro ao abrir %s.\n", ARQ_VENDAS);
}

/* Cancelar/remover venda: devolve os itens ao estoque e remove o registro. */
static void remover_venda(void) {
    int id = ler_int("ID da venda a remover (cancelar): ");
    Venda v;
    if (buscar_venda(id, &v) < 0) {
        printf("Venda %d nao encontrada.\n", id);
        return;
    }

    int r = remover_registro(ARQ_VENDAS, sizeof(Venda), id);
    if (r != 1) {
        printf("Erro ao remover a venda.\n");
        return;
    }

    for (int i = 0; i < v.qtd_itens; i++) {
        Produto p;
        int pos = buscar_produto(v.itens[i], &p);
        if (pos >= 0) {            /* produto pode ja ter sido removido */
            p.estoque += v.quantidades[i];
            gravar_produto_pos(pos, &p);
        }
    }
    printf("Venda removida e itens devolvidos ao estoque.\n");
}

/* ------------------------------------------------------------------ */
/* Relatorios                                                          */
/* ------------------------------------------------------------------ */

/* Cada venda com a DESCRICAO dos produtos vendidos (buscada em produtos.dat). */
static void relatorio_vendas_produtos(void) {
    FILE *f = fopen(ARQ_VENDAS, "rb");
    if (!f) {
        printf("Nenhuma venda registrada.\n");
        return;
    }
    Venda v;
    int qtd = 0;
    float geral = 0.0f;
    printf("\n===== RELATORIO: VENDAS E SEUS PRODUTOS =====\n");
    while (fread(&v, sizeof(Venda), 1, f) == 1) {
        qtd++;
        printf("\nVenda #%d - Cliente: %s - Data: %s\n", v.id, v.cliente, v.data);
        for (int i = 0; i < v.qtd_itens; i++) {
            Produto p;
            float sub = v.precos[i] * (float)v.quantidades[i];
            if (buscar_produto(v.itens[i], &p) >= 0)
                printf("   - %-30s %3d x R$ %7.2f = R$ %8.2f\n",
                       p.descricao, v.quantidades[i], v.precos[i], sub);
            else
                printf("   - %-30s %3d x R$ %7.2f = R$ %8.2f\n",
                       "[produto removido]", v.quantidades[i], v.precos[i], sub);
        }
        printf("   TOTAL DA VENDA: R$ %.2f\n", v.total);
        geral += v.total;
    }
    fclose(f);
    if (qtd == 0)
        printf("Nenhuma venda registrada.\n");
    else
        printf("\nTotal de vendas: %d | Faturamento geral: R$ %.2f\n", qtd, geral);
}

/* Cada produto com estoque atual e total vendido (cruzando com vendas.dat). */
static void relatorio_estoque(void) {
    FILE *fp = fopen(ARQ_PRODUTOS, "rb");
    if (!fp) {
        printf("Nenhum produto cadastrado.\n");
        return;
    }
    Produto p;
    int qtd = 0;
    printf("\n===== RELATORIO: CONTROLE DE ESTOQUE =====\n");
    while (fread(&p, sizeof(Produto), 1, fp) == 1) {
        qtd++;
        int vendido = 0;
        int nvendas = 0;

        FILE *fv = fopen(ARQ_VENDAS, "rb");
        if (fv) {
            Venda v;
            while (fread(&v, sizeof(Venda), 1, fv) == 1) {
                for (int i = 0; i < v.qtd_itens; i++) {
                    if (v.itens[i] == p.id) {
                        vendido += v.quantidades[i];
                        nvendas++;
                        break;
                    }
                }
            }
            fclose(fv);
        }

        printf("\nProduto #%d - %s (R$ %.2f)\n", p.id, p.descricao, p.preco);
        printf("   Estoque atual: %d%s\n", p.estoque,
               p.estoque <= ESTOQUE_BAIXO ? "  <-- ESTOQUE BAIXO" : "");
        printf("   Total vendido: %d unidade(s) em %d venda(s)\n", vendido, nvendas);
        if (nvendas > 0) {
            printf("   Clientes: ");
            FILE *fv2 = fopen(ARQ_VENDAS, "rb");
            if (fv2) {
                Venda v;
                int primeiro = 1;
                while (fread(&v, sizeof(Venda), 1, fv2) == 1) {
                    for (int i = 0; i < v.qtd_itens; i++) {
                        if (v.itens[i] == p.id) {
                            printf("%s%s", primeiro ? "" : ", ", v.cliente);
                            primeiro = 0;
                            break;
                        }
                    }
                }
                fclose(fv2);
            }
            printf("\n");
        }
    }
    fclose(fp);
    if (qtd == 0) printf("Nenhum produto cadastrado.\n");
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */

static void menu_produtos(void) {
    int op;
    do {
        printf("\n--- PRODUTOS ---\n"
               "1 - Cadastrar\n2 - Listar todos\n3 - Consultar por ID\n"
               "4 - Atualizar\n5 - Remover\n0 - Voltar\n");
        op = ler_int("Opcao: ");
        switch (op) {
        case 1: cadastrar_produto();  break;
        case 2: listar_produtos();    break;
        case 3: consultar_produto();  break;
        case 4: atualizar_produto();  break;
        case 5: remover_produto();    break;
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

static void menu_vendas(void) {
    int op;
    do {
        printf("\n--- VENDAS ---\n"
               "1 - Registrar venda\n2 - Listar todas\n3 - Consultar por ID\n"
               "4 - Atualizar (cliente/data)\n5 - Remover (cancelar)\n0 - Voltar\n");
        op = ler_int("Opcao: ");
        switch (op) {
        case 1: registrar_venda();  break;
        case 2: listar_vendas();    break;
        case 3: consultar_venda();  break;
        case 4: atualizar_venda();  break;
        case 5: remover_venda();    break;
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

static void menu_relatorios(void) {
    int op;
    do {
        printf("\n--- RELATORIOS ---\n"
               "1 - Vendas e seus produtos\n"
               "2 - Controle de estoque (com vendas por produto)\n0 - Voltar\n");
        op = ler_int("Opcao: ");
        switch (op) {
        case 1: relatorio_vendas_produtos(); break;
        case 2: relatorio_estoque();         break;
        case 0: break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
}

int main(void) {
    int op;
    do {
        printf("\n===== VENDAS E CONTROLE DE ESTOQUE =====\n"
               "1 - Produtos\n2 - Vendas\n3 - Relatorios\n0 - Sair\n");
        op = ler_int("Opcao: ");
        switch (op) {
        case 1: menu_produtos();   break;
        case 2: menu_vendas();     break;
        case 3: menu_relatorios(); break;
        case 0: printf("Ate logo!\n"); break;
        default: printf("Opcao invalida.\n");
        }
    } while (op != 0);
    return 0;
}
