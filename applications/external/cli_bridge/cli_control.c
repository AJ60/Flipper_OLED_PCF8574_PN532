#include "cli_control.h"

#include <FreeRTOS.h>
#include <cli/cli_vcp.h>
#include <furi.h>

FuriStreamBuffer* cli_tx_stream = NULL;
FuriStreamBuffer* cli_rx_stream = NULL;

void clicontrol_hijack(size_t tx_size, size_t rx_size) {
    if(cli_rx_stream != NULL || cli_tx_stream != NULL) {
        return;
    }

    cli_rx_stream = furi_stream_buffer_alloc(rx_size, 1);
    cli_tx_stream = furi_stream_buffer_alloc(tx_size, 1);

    CliVcp* cli_vcp = furi_record_open(RECORD_CLI_VCP);
    cli_vcp_disable(cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
}

void clicontrol_unhijack(bool persist) {
    UNUSED(persist);

    if(cli_rx_stream == NULL && cli_tx_stream == NULL) {
        return;
    }

    if(cli_rx_stream) {
        furi_stream_buffer_free(cli_rx_stream);
        cli_rx_stream = NULL;
    }

    if(cli_tx_stream) {
        furi_stream_buffer_free(cli_tx_stream);
        cli_tx_stream = NULL;
    }

    CliVcp* cli_vcp = furi_record_open(RECORD_CLI_VCP);
    cli_vcp_enable(cli_vcp);
    furi_record_close(RECORD_CLI_VCP);
}